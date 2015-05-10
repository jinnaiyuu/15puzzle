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

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <iostream>
#include <stdint.h>

// Burns' implementation: block-14 5 sec in A*.
struct Strips {

	// TODO: would 16 bit enough
	struct State {
		std::vector<unsigned int> propositions; // need to be sorted
		int h;
	};

	struct PackedState {
		unsigned long word;
		std::vector<unsigned int> propositions;

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
	};

	// Tiles constructs a new instance by reading
	// the initial state from the given file which is
	// expected to be in Wheeler's tiles instance
	// format.
	Strips(std::istream &domain, std::istream &instance);
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
		std::vector<unsigned int> p = s.propositions;
		for (int i = 0; i < goal_condition.size(); ++i) {
			if (std::find(p.begin(), p.end(), goal_condition[i]) != p.end()) {
				continue;
			} else {
				return false;
			}
		}
		return true;
	}

	int nops(const State &s) const {
		std::vector<unsigned int> actions = actionTrie.searchPossibleActions(
				s.propositions);
		return actions.size();
	}

	// TODO: nops&nthop is duplication of work for STRIPS planning. should be optimized.
	int nthop(const State &s, int n) const {
		std::vector<unsigned int> actions = actionTrie.searchPossibleActions(
				s.propositions);
		return actions[n];
	}

	struct Undo {
		int action;
		int h;
	};

	Edge<Strips> apply(State &s, int action) const {
		int cost = 1;
		Edge<Strips> e(cost, action, -100);
		e.undo.action = action;
		e.undo.h = s.h;

//		std::cout << "apply " << actionTable.getAction(action).name << std::endl;
//		print_state(s.propositions);
		apply_action(s, action);
//		print_state(s.propositions);

		s.h = heuristic(s);

		return e;
	}

	// invert the add&delete effect of the action
	void undo(State &s, const Edge<Strips> &e) const {
		s.h = e.undo.h;
		undo_action(s, e.undo.action);
	}

	// pack: packes state s into the packed state dst.
	void pack(PackedState &dst, State &s) const {
		// TODO: should optimize binary length.
		dst.word = 0;
		for (int i = 0; i < s.propositions.size(); i++) {
			dst.word = (dst.word << 4) | s.propositions[i];
		}
		dst.propositions = s.propositions; // copy
	}

	// unpack: unpacks the packed state s into the state dst.
	void unpack(State &dst, PackedState s) const {
		dst.propositions = s.propositions;
		dst.h = heuristic(dst);
	}

	void set_heuristic(int h) {
		which_heuristic = h;
	}

private:
	// proposition library (prefixtree)
	// action library (table of number to action)

	int which_heuristic; // 0:blind, 1:goal_count, 2:hmax
	Trie actionTrie; // dictionary which returns feasible actions for given state.
	ActionTable actionTable; // table of action_key to action description.
	std::vector<unsigned int> init_state;
	std::vector<unsigned int> goal_condition;

	void apply_action(State& s, int action_key) const {
		Action action = actionTable.getAction(action_key);
		for (int i = 0; i < action.adds.size(); ++i) {
			s.propositions.push_back(action.adds[i]);
		}
		for (int i = 0; i < action.deletes.size(); ++i) {
			s.propositions.erase(
					std::remove(s.propositions.begin(), s.propositions.end(),
							action.deletes[i]), s.propositions.end());
		}
		std::sort(s.propositions.begin(), s.propositions.end());
	}

	void undo_action(State& s, int action_key) const {
		Action action = actionTable.getAction(action_key);
		for (int i = 0; i < action.deletes.size(); ++i) {
			s.propositions.push_back(action.deletes[i]);
		}
		for (int i = 0; i < action.adds.size(); ++i) {
			s.propositions.erase(
					std::remove(s.propositions.begin(), s.propositions.end(),
							action.adds[i]), s.propositions.end());
		}
		std::sort(s.propositions.begin(), s.propositions.end());
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
		int goals = goal_condition.size();
		std::vector<unsigned int> p = s.propositions;
		for (int i = 0; i < goal_condition.size(); ++i) {
			if (std::find(p.begin(), p.end(), goal_condition[i]) != p.end()) {
				--goals;
			}
		}
		return goals;
	}

	int hmax(const State& s) const {
		assert(false);
		return 0;
	}

	/////////////////////////////
	// parser
	/////////////////////////////

	// Predicates will be grounded after this initialization.
public:
	struct Predicate {
		unsigned int key;
		std::string symbol;
		unsigned int number_of_arguments;
		Predicate() {
		}
		;
		Predicate(unsigned int key, std::string symbol,
				unsigned int number_of_arguments) :
				key(key), symbol(symbol), number_of_arguments(
						number_of_arguments) {
		}

	};

	struct Object {
		unsigned int key;
		std::string symbol;
	};

	struct GroundedPredicate {
		unsigned int key;
		std::string symbol;

		unsigned int lifted_key;
		std::string lifted_symbol;
		std::vector<unsigned int> arguments;
	};

	std::vector<GroundedPredicate> g_predicates;

	void print_plan(std::vector<unsigned int>& path) const;
	void print_state(std::vector<unsigned int>& propositions) const;


private:
	void readPredicates(std::istream &domain,
			std::vector<Predicate>& predicates);
	void readObjects(std::istream &instance, std::vector<Object>& objects);
	void ground(Predicate& p, std::vector<unsigned int> argv, unsigned int key,
			GroundedPredicate& g, std::vector<Object>& obs);
	void groundPredicates(std::vector<Predicate>& ps, std::vector<Object>& obs,
			std::vector<GroundedPredicate>& gs);
	void readInit(std::istream &instance, std::vector<GroundedPredicate>& gs);
	void readGoal(std::istream &instance, std::vector<GroundedPredicate>& gs);
	void readAction(std::istream &domain, std::vector<Object> obs,
			std::vector<GroundedPredicate> gs);
	void listFeasiblePredicates(std::vector<unsigned int>& gs);
	void listFeasibleActions(std::vector<unsigned int> gs,
			std::vector<unsigned int>& actions);
	void buildActionTrie(std::vector<unsigned> keys);
//	bool getText(std::istream &file, std::string from, std::string to, unsigned int number, std::string& ret);
	int pow(int base, int p);

};
#endif