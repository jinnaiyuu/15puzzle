#ifndef STRIPS_HPP_
#define STRIPS_HPP_

// Copyright 2012 Ethan Burns. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include "action_table.hpp"
#include "trie.hpp"

#include "../search.hpp"
#include "../fatal.hpp"
#include "../hashtbl.hpp"

// for pattern databases
#include "pdb.hpp"

#include "../dist_hash.hpp"
#include "zobrist.hpp"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <iostream>
#include <stdint.h>

//typedef unsigned int feature;

/**
 * Possible Optimizations
 * 1. std::find from ordered vector (all predicates are sorted)
 * 2. static predicates
 * 3. incremental zobrist
 * 4. duplication of nops & nthop
 * 5.
 *
 */
struct Strips {

	// TODO: would 16 bit enough
	struct State {
		std::vector<unsigned int> propositions; // need to be sorted

//		std::vector<unsigned int> propositions; // need to be sorted
		int h;
	};

	struct PackedState {
		unsigned long word;
		std::vector<unsigned int> propositions;
		unsigned int h;

		unsigned long hash() const {
			return word;
		}

		// however long you take for the hash, it does not cover all the sequences.
		bool eq(const PackedState &h) const {
//			return word == h.word;
			for (unsigned int i = 0; i < h.propositions.size(); ++i) {
				if (propositions[i] != h.propositions[i]) {
					return false;
				}
			}
			return true;
		}

		unsigned int byteSize() const {
			return 4 + 4 + propositions.size() * 4 + 4;
		}

		template<typename T>
		void intToChars(T& p, unsigned char* d) const {
			int n = sizeof p;
			for (int y = 0; n-- > 0; y++)
				d[y] = (p >> (n * 8)) & 0xff;
		}

		template<typename T>
		void charsToData(T& p, unsigned char* d) {
			int n = sizeof p;
			p = 0;
			for (int i = 0; i < n; ++i) {
				p += (T) d[i] << (8 * (n - i - 1));
			}
		}

		void stateToChars(unsigned char* d) const {
			intToChars(word, &(d[0]));
			int size = propositions.size();
			intToChars(size, &(d[4]));
			for (int i = 0; i < size; ++i) {
				intToChars(propositions[i], &(d[i * 4 + 8]));
			}
			intToChars(h, &(d[size * 4 + 8]));
		}

		void charsToState(unsigned char* d) {
			charsToData(word, &(d[0]));
			int size = 0;
			charsToData(size, &(d[4]));
			propositions.resize(size);
			for (int i = 0; i < size; ++i) {
				charsToData(propositions[i], &(d[i * 4 + 8]));
			}
			charsToData(h, &(d[size * 4 + 8]));
		}
	};

	// Tiles constructs a new instance by reading
	// the initial state from the given file which is
	// expected to be in Wheeler's tiles instance
	// format.
	Strips(std::istream &domain, std::istream &instance);
	Strips();
	~Strips() {
	}

	State initial() const {
		State s;
		s.propositions = init_state;
		s.h = heuristic(s);
		return s;
	}

	int h(const State &s) const {
		return s.h;
	}

	bool isgoal(const State &s) const {
		// TODO: should be optimized. 2 sequences are sorted.
		//	std::cout << "isgoal" << std::endl;
		std::vector<unsigned int> p = s.propositions;
		return !isAnyNotContainedSortedVectors(goal_condition, s.propositions);
	}

	std::vector<unsigned int> ops(const State &s) const {
//		std::cout << "nops" << std::endl;
//		actionTrie.printTree();
//		std::vector<unsigned int> actions = actionTrie.searchPossibleActions(
//				s.propositions);
//		std::cout << "expand state: " << actions.size() << std::endl;
		return actionTrie.searchPossibleActions(s.propositions);
	}

	int nops(const State &s) const {
//		std::cout << "nops" << std::endl;
//		actionTrie.printTree();
		std::vector<unsigned int> actions = actionTrie.searchPossibleActions(
				s.propositions);
//		std::cout << "expand state: " << actions.size() << std::endl;
		return actions.size();
	}

	// TODO: nops&nthop is duplication of work for STRIPS planning. should be optimized.
	int nthop(const State &s, int n) const {
//		std::cout << "nthop" << std::endl;
//		print_state(s.propositions);
		std::vector<unsigned int> actions = actionTrie.searchPossibleActions(
				s.propositions);
//		std::cout << "action size = " << actions.size() << std::endl;
		return actions[n];
	}

	struct Undo {
		std::vector<unsigned int> propositions;
		int h;
	};

	Edge<Strips> apply(State &s, int action) const {
//		std::cout << "apply" << std::endl;
		int cost = 1;
		std::vector<unsigned int> p = s.propositions;
		int h = s.h;

//		std::cout << "apply " << actionTable.getAction(action).name << std::endl;
//		print_state(s.propositions);
		apply_action(s, action, cost);

		Edge<Strips> e(cost, action, -1);
		e.undo.propositions = p;
		e.undo.h = h;
//		print_state(s.propositions);

		s.h = heuristic(s);

		return e;
	}

	// invert the add&delete effect of the action
	void undo(State &s, const Edge<Strips> &e) const {
		//		std::cout << "undo" << std::endl;
		s.h = e.undo.h;
		undo_action(s, e.undo);
	}

	// pack: packes state s into the packed state dst.
	void pack(PackedState &dst, State &s) const {
		//		std::cout << "pack" << std::endl;
		// TODO: should optimize binary length.
		//       how can we build an appropriate hash?
		dst.word = hash(s.propositions);
//		for (int i = 0; i < s.propositions.size(); i++) {
////			dst.word = (dst.word << 4) | s.propositions[i];
//			hash_combine(dst.word, s.propositions[i]);
//		}
		dst.propositions = s.propositions; // copy
		dst.h = s.h;
	}

	// unpack: unpacks the packed state s into the state dst.
	void unpack(State &dst, PackedState s) const {
		//	std::cout << "unpack" << std::endl;
		dst.propositions = s.propositions;
		dst.h = s.h;
	}

	void set_heuristic(int h) {

		which_heuristic = h;
		if (h == 2) {
			// hmax heuristics
//			strips.resize(goal_condition.size());
//			for (int i = 0; i < strips.size(); ++i) {
//				strips[i] = new Strips();

//			}
//			search = new Astar<Strips>(*strips);
//			strips->set_heuristic(1);
		}
		if (h == 3) {
			// pattern database heuristic
			buildPDB();
		}
	}

	// 0 | f | use pdb if available
	// 1 | t | always build
	// 2 | t | always build and don't search
	// 3 | f | build if no pdb available
	// 4 | f | use pdb if available, if no pdb then terminate.
	void set_pdb(unsigned int pdb) {
		built_pdb = pdb;
//		if (pdb == 3) {
//			built_pdb = false;
//		}
	}

	void set_pdb_size(unsigned int pdb_size) {
		this->pdb_size = pdb_size;
	}

	void set_dist_hash(int h, unsigned int rand_seed = 0, double toStructure = 0.3) {
		which_dist_hash = h;
		dist_h = new StripsZobrist<Strips>(*this, which_dist_hash, rand_seed, toStructure);
	}

	unsigned int dist_hash(const State &s) const {
		return dist_h->dist_h(s);
	}











	unsigned int getActionSize() {
		return actionTable.getSize();
	}

	unsigned int getGroundedPredicatesSize() {
		return g_predicates->size();
	}

	struct GroundedPredicate {
		unsigned int key;
		std::string symbol;

		unsigned int lifted_key;
		std::string lifted_symbol;
		std::vector<unsigned int> arguments;

		bool isEqual(unsigned int lifted_key,
				std::vector<unsigned int> arguments) {
			return (this->lifted_key == lifted_key)
					&& (this->arguments == arguments);
		}

		bool matches(unsigned int lifted_key, unsigned int argposition,
				unsigned int argvalue) {
			return (this->lifted_key == lifted_key)
					&& (this->arguments[argposition] == argvalue);
		}

		int argValue(unsigned int lifted_key, unsigned int argposition) {
			if (this->lifted_key == lifted_key) {
				return arguments[argposition];
			} else {
				return -1;
			}
		}
	};

	// this copies only a pointer.
	void setActionTrie(Trie trie) {
		this->actionTrie = trie;
	}

	// this copies only a pointer.
	void setActionTable(ActionTable table) {
		this->actionTable = table;
	}

	// this copies only a pointer.
	ActionTable* getActionTable() {
		return &(this->actionTable);
	}

	void setInitState(std::vector<unsigned int> init_state) {
		this->init_state = init_state;
	}

	void setGoalCondition(std::vector<unsigned int> goal_condition) {
		this->goal_condition = goal_condition;
	}

	// this copies only a pointer.
	void setGroundedPredicates(std::vector<GroundedPredicate>* gs) {
		this->g_predicates = gs;
	}

	void setIsDeleteRelaxed(bool is) {
		is_delete_relaxed = is;
	}

	//////////////////////////////////////////
	/// methods for regression planning
	//////////////////////////////////////////
	std::vector<unsigned int> r_nops(const State &s) const {
		// TODO: Add ungroupeds in propositions.
		std::vector<unsigned int> ps = uniquelyMergeSortedVectors(
				s.propositions, ungroupeds);
		std::vector<unsigned int> actions = rActionTrie.searchPossibleActions(
				ps);
		return actions;
	}

	// TODO: nops&nthop is duplication of work for STRIPS planning. should be optimized.
	int r_nthop(const State &s, int n) const {
		// TODO: Add ungroupeds in propositions.
		std::vector<unsigned int> ps = uniquelyMergeSortedVectors(
				s.propositions, ungroupeds);
		std::vector<unsigned int> actions = rActionTrie.searchPossibleActions(
				ps);
		return actions[n];
	}

	Edge<Strips> r_apply(State &s, int action) const {
//		std::cout << "apply" << std::endl;
		int cost = 1;
		Edge<Strips> e(cost, action, -100);
		e.undo.propositions = s.propositions;
		e.undo.h = s.h;

//		std::cout << "apply " << actionTable.getAction(action).name << std::endl;
//		print_state(s.propositions);
		r_apply_action(s, action);
//		print_state(s.propositions);

//		s.h = heuristic(s);

		return e;
	}

	bool pdb_is_built = true;

private:
	// proposition library (prefixtree)
	// action library (table of number to action)

	int which_heuristic; // 0:blind, 1:goal_count, 2:hmax
	Trie actionTrie; // dictionary which returns feasible actions for given state.
	ActionTable actionTable; // table of action_key to action description.
	std::vector<unsigned int> init_state;
	std::vector<unsigned int> goal_condition;
	std::vector<GroundedPredicate>* g_predicates;
	std::vector<unsigned int> g_predicates_index; // index for head lifted predicate key in g_predicates.
//	GroundedPredicateTable g_predicatess;

	bool is_delete_relaxed = false;

	Trie rActionTrie; // trie for regression planning. keys are adds.
	std::vector<unsigned int> ungroupeds;
	unsigned int built_pdb = 0;
	unsigned int pdb_size = 1000000;

	// TODO: add new utility method to speed-up this.
	void apply_action(State& s, int action_key, int& cost) const {
		std::vector<unsigned int> p;
		Action* action = actionTable.getActionR(action_key);
//		for (int i = 0; i < action.adds.size(); ++i) {
//			s.propositions.push_back(action.adds[i]);
//		}

//		std::vector<unsigned int>::iterator it;
//
//		it = std::set_difference(s.propositions.begin(), s.propositions.end(),
//				action->deletes.begin(), action->deletes.end(),
//				s.propositions.begin());
//		it = std::set_union(s.propositions.begin(), it, action->adds.begin(),
//				action->adds.end(), s.propositions.begin());
//
//		s.propositions.resize(it - s.propositions.begin());

		std::set_difference(s.propositions.begin(), s.propositions.end(),
				action->deletes.begin(), action->deletes.end(),
				std::back_inserter(p));
		s.propositions.clear();
		std::set_union(p.begin(), p.end(), action->adds.begin(),
				action->adds.end(), std::back_inserter(s.propositions));

//		p = uniquelyMergeSortedVectors(s.propositions, action.adds);

		// TODO: is this correct?
//		if (!is_delete_relaxed) {
//			std::vector<unsigned int> d;
//			p = differenceSortedVectors(p, action.deletes);
//			p = d;
//		}

//		s.propositions = differenceSortedVectors(s.propositions,
//				action->deletes);
//		s.propositions.insert(s.propositions.end(), action->adds.begin(),
//				action->adds.end());
//		std::sort(s.propositions.begin(), s.propositions.end());
//		s.propositions.erase(
//				unique(s.propositions.begin(), s.propositions.end()),
//				s.propositions.end());

//		s.propositions = p;
		cost = action->action_cost;
	}

	// TODO: can we optimize this?
	// In regression model,
	// ADD: preconditions
	// DEL: add effects
	void r_apply_action(State& s, int action_key) const {
		Action action = actionTable.getAction(action_key);

		for (int i = 0; i < action.adds.size(); ++i) {
			s.propositions.erase(
					std::remove(s.propositions.begin(), s.propositions.end(),
							action.adds[i]), s.propositions.end());
		}

		std::vector<unsigned int> n = uniquelyMergeSortedVectors(s.propositions,
				action.preconditions);
		s.propositions = n;
	}

	void undo_action(State& s, const Undo& undo) const {
		s.propositions = undo.propositions;
	}

	/////////////////////////////
	// heuristics
	/////////////////////////////
	int heuristic(const State& s) const {
		switch (which_heuristic) {
		case 0: // blind
			return blind(s);
			break;
		case 1: // goal_count
			return goal_count(s);
			break;
		case 2: // hmax
			return hmax(s);
			break;
		case 3: // pattern database
			return pdb(s);
			break;
		default:
			assert(false);
			break;
		}
		return 0;
	}

	int blind(const State& s) const {
		return 0;
	}

	int goal_count(const State& s) const {
		// TODO: optimize
		int goals = goal_condition.size();
		std::vector<unsigned int> p = s.propositions;
		for (int i = 0; i < goal_condition.size(); ++i) {
			if (std::find(p.begin(), p.end(), goal_condition[i]) != p.end()) {
				--goals;
			}
		}
		return goals;
	}

	int hmax(const State& s) const;

	int pdb(const State& s) const;

	void buildPDB();
	void dijkstra(std::vector<std::vector<unsigned int> > patterns,
			std::vector<std::vector<unsigned int> > groups);
	void buildRegressionTree();

	/////////////////////////////
	// parser
	/////////////////////////////

	// Predicates will be grounded after this initialization.
public:
	struct Predicate {
		unsigned int key;
		std::string symbol;
		unsigned int number_of_arguments;
		std::vector<unsigned int> types_of_arguments;
		int group_key;
		bool isStatic;
		Predicate() {
		}
		;
		Predicate(unsigned int key, std::string symbol,
				unsigned int number_of_arguments) :
				key(key), symbol(symbol), number_of_arguments(
						number_of_arguments), group_key(-1), isStatic(false) {
		}

	};

	struct PredicateArg {
		unsigned int key;
		Predicate pred;
		unsigned int instantiated_arg;
		int group_key;
		bool isInGoal;

		bool isEqual(unsigned int pred_key, unsigned int inst_arg) {
			return pred.key == pred_key && instantiated_arg == inst_arg;
		}

		bool matches(const GroundedPredicate& g, unsigned int obj) {
			return pred.key == g.lifted_key
					&& g.arguments[instantiated_arg] == obj;
		}

		bool matchesLiftedKey(const GroundedPredicate& g) {
			return pred.key == g.lifted_key;
		}

	};

	struct Object {
		unsigned int key;
		std::string symbol;
		int type;
	};

	struct LiftedAction {
		unsigned int key;
		std::string symbol;
		unsigned int n_arguments;

		// lifted predicate key & arguments.
		std::vector<std::pair<unsigned int, std::vector<unsigned int>>>adds;
		std::vector<std::pair<unsigned int, std::vector<unsigned int>>> dels;
		std::vector<std::pair<unsigned int, std::vector<unsigned int>>> precs;

		std::vector<unsigned int> addsInst; // PredicateArg keys.
		std::vector<unsigned int> delsInst;
		unsigned int action_cost;

		std::vector<std::pair<std::string, int>> param;
	};

	struct LiftedActionArg {
		unsigned int key;
		LiftedAction lf;
		unsigned int instantiated_arg; // which argument is instantiated
	};

	const std::vector<std::vector<unsigned int>> get_structures();
	const std::vector<std::vector<unsigned int>> get_xor_groups();

	void print_plan(std::vector<State>& path) const;
	void print_state(const std::vector<unsigned int>& propositions) const;
	void print_state(const State& s) const;

private:

	const unsigned int TRUE_PREDICATE = 10000000;

	unsigned int which_dist_hash;
	DistributionHash<Strips>* dist_h;

	bool typing = false;
	bool action_costs = false;
	bool need_heap_openlist = false;
	std::vector<Object> objects;
	std::vector<LiftedAction> lActions; // for abstractions.
	std::vector<Predicate> predicates;// uninstantiated, ungrounded
	std::vector<PredicateArg> predargs;
	std::vector<unsigned int> g_feasible_predicates;
	std::vector<std::vector<unsigned int>> xor_groups;
	std::vector<unsigned int> xor_ungroupeds;
	std::vector<std::vector<unsigned int>> structures;// structures are made of xor_groups.

	std::vector<std::pair<std::string, int>> types;
	std::vector<std::pair<std::string, std::string>> constants;

	PDB* pd;// need deallocate
	int n_groups = 0;

	std::string domain_;
	std::string instance_;
	void readDomainName(std::istream &domain);
	void readInstanceName(std::istream &instance);

	void readRequirements(std::istream &domain);
	void readPredicates(std::istream &domain,
			std::vector<Predicate>& predicates);
	std::vector<std::pair<std::string, int>> readTypes(std::istream& domain);
	void readObjects(std::istream &instance);
	void ground(Predicate& p, std::vector<unsigned int> argv, unsigned int key,
			GroundedPredicate& g, std::vector<Object>& obs);
	void groundPredicates(std::vector<Predicate>& ps, std::vector<Object>& obs,
			std::vector<GroundedPredicate>& gs);
	void readInit(std::istream &instance, std::vector<GroundedPredicate>& gs);
	void readGoal(std::istream &instance, std::vector<GroundedPredicate>& gs);
	void readAction(std::istream &domain, std::vector<Object> obs,
			std::vector<Predicate> ps,
			std::vector<GroundedPredicate> gs);
	void analyzeStaticLiftedPredicates();
	void groundAction(std::istream &domain, std::vector<Object> obs,
			std::vector<Predicate> ps,
			std::vector<GroundedPredicate> gs);
	void listFeasiblePredicates(std::vector<unsigned int>& gs);
	void listFeasiblePredicates2(std::vector<unsigned int>& gs);
	void listFeasiblePredicatesWithActions(std::vector<unsigned int>& gs, std::vector<unsigned int>& actions);
	void listFeasibleActions(std::vector<unsigned int> gs,
			std::vector<unsigned int>& actions);
	void buildActionTrie(std::vector<unsigned> keys);
	unsigned int match_pattern(std::vector<unsigned int> p, const std::vector<std::vector<unsigned int>>& groups);

	std::vector<unsigned int> analyzeBalance(unsigned int p,
			const std::vector<unsigned int>& predicates,
			const std::vector<LiftedActionArg>& laas);
	void analyzeAllBalances(std::vector<Predicate> ps);
	void analyzeXORGroups();
	void getConstants(std::istream& domain, std::vector<std::pair<std::string, std::string>>& type_object,
			std::string header, bool whole_text = false);
	unsigned int groundedPredicateKey(const std::pair<unsigned int, std::vector<unsigned int> >& pred);

	int pow(int base, int p);

	void buildStructure();

public:
	// this is for structured zobrist hash.
	void analyzeTransitions();

};
#endif
