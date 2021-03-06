// Copyright 2012 Ethan Burns. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#ifndef _HASHTBL_HPP_
#define _HASHTBL_HPP_

#include <vector>
#include <stdio.h>
#include <pthread.h>
#include <iostream>
#include "pool.hpp"

template<class Node> struct HashEntry {
	unsigned long hash;
	Node *next;
};

// HashTable implements a simple single-sized hash table.
template<class Key, class Node> class HashTable {

	std::vector<Node*> buckets;
//	pthread_mutex_t m;

public:

	// buckets are all NULL pointers.
	HashTable(unsigned int sz) :
			buckets(sz, 0) {
//		for (int i = 0; i < sz; ++i) {
//			if (buckets[i] != NULL) {
//				std::cout << "non null" << std::endl;
//			}
//		}
//		pthread_mutex_init(&m, NULL);
	}

	void destruct_all(Pool<Node>& nodes) {
		for (int i = 0; i < buckets.size(); ++i) {
			Node* n = buckets[i];
			destruct(n, nodes);
		}
	}

	void destruct(Node* n, Pool<Node>& nodes) {
		Node* next = n->hashentry().next;
		destruct(next, nodes);
		nodes.destruct(n);
	}

	// find looks up the given key in the hash table and returns
	// the data value if it is found or else it returns 0.
	// TODO: strange error occurring here.
	//       possibly null input?
	Node *find(Key &key) {
		unsigned long h = key.hash();
		unsigned int ind = h % buckets.size();

		Node *p;
//		try {
//		for (p = buckets[ind]; p && p->key().eq(key); p = p->hashentry().next) {
		for (p = buckets[ind]; p; p = p->hashentry().next) {
			// p is not NULL
//			std::string s = "";
//			Node n = *p;
//			char c[7];
//			sprintf(c, "%u, %u\n", n.f, n.g);
//			std::string s(c);
//			std::cout << s;
			Key k = p->key();
//				unsigned int = p->key().hash();
//				if (p->hashentry().hash == h) {
//					break;
//				}
			if (k.eq(key)) {
				break;
			}
//			if (p->key().eq(key)) {
//				std::cout << "eq" << std::endl;
//			} else {
//				std::cout << "uneq" << std::endl;
//			}
			;
		}
//		} catch (...) {
//			std::cout << "p->key() = " << p->key().hash() << std::endl;
//			std::cout << "key = " << key.hash() << std::endl;
//
//		}

		return p;
	}

	// add adds a value to the hash table.
	// it will insert the node in front of the list.
	void add(Node *n) {
//		pthread_mutex_lock(&m);
		unsigned long hash = n->key().hash();
		unsigned int ind = hash % buckets.size();
		n->hashentry().hash = hash;
		n->hashentry().next = buckets[ind];
		buckets[ind] = n;
		//	pthread_mutex_unlock(&m);
	}
};

#endif	// _HASHTBL_HPP_
