/*
 * zobrist.h
 *
 *  Created on: Jun 10, 2014
 *      Author: yuu
 */

#ifndef ZOBRIST_H_
#define ZOBRIST_H_

#include <cmath>
#include <climits>
#include <cstdlib>
#include <math.h>

template<typename D, int size>
class Zobrist {
public:
	enum ABST {
		SINGLE = 0,
		PAIR = 1,
		LINE = 2,
		BLOCK = 3,
		TWO = 4,
		ABSTRACTION = 123,
		FAR_TILES = 1712,
		FOURBLOCK_24 = 4024,
		FOURABSTRACTION = 1234
	};

// Should delete compatibility for performance.
	Zobrist(int tnum_ = 1, ABST abst = SINGLE) :
			tnum(tnum_) {
		initZobrist(abst);
//		dump_table();
	}

	Zobrist(D tnum_ , ABST abst = SINGLE)
			{
		initZobrist(abst);
//		dump_table();
	}

	/**
	 * @param number: The number slided
	 * @param from : Where the number is in parent node
	 * @param to : Where the number is in child node
	 * @return the value to XOR to the Zobrist value of parent.
	 */
	unsigned int inc_hash(const unsigned int previous, const int number,
			const int from, const int to, const char* const newBoard, const typename D::State s) const {
		return (previous ^ inc_zbr[number][from][to]);
	}

	unsigned int inc_hash(typename D::State s) const {
		return 0;
	}


	unsigned int hash_tnum(const char* const board) const {
		return hash(board) % tnum;
	}

//#define RANDOM_ZOBRIST_INITIALIZATION
private:
	void initZobrist(ABST abst) {
		gen = std::mt19937(rd());
//		unsigned int max = std::numeric_limits<hashlength>::max();
//		unsigned int max = UINT_MAX;
		dis = std::uniform_int_distribution<>(INT_MIN, INT_MAX);
// Not sure I should initialize it by time as it randomize the results for each run.
#ifdef RANDOM_ZOBRIST_INITIALIZATION
		srand(time(NULL));
#endif
		for (int j = 0; j < size; ++j) {
			zbr[0][j] = 0;
		}

		// TODO: Here, we can implement some kind of tricks for load balancing.
		// 1. Abstraction
		// 2. Something from chess programming
		switch (abst) {
		case SINGLE:
			single();
			break;
		case PAIR:
			pair();
			break;
		case LINE:
			line();
			break;
		case BLOCK:
			block();
			break;
		case TWO:
			two();
			break;
		case ABSTRACTION:
			abstraction();
			break;
		case FAR_TILES:
			far_tiles_abstraction();
			break;
		case FOURBLOCK_24:
			four24();
			break;
		case FOURABSTRACTION:
			fourabstraction();
			break;
		default:
			printf("ERRRRRRRORRRRR\n");
			break;
		}

		for (int i = 1; i < size; ++i) { // num
			for (int j = 0; j < size; ++j) { // from
				for (int k = 0; k < size; ++k) { // to
					inc_zbr[i][j][k] = zbr[i][j] ^ zbr[i][k];
				}
			}
		}

//		dump_table();
	}

	void single() {
		for (int i = 1; i < size; ++i) {
			for (int j = 0; j < size; ++j) {
				zbr[i][j] = random();
			}
		}
	}

	void pair() {
		for (int i = 1; i < size; ++i) {
			for (int j = 0; j < size; j += 2) {
				int r = random();
				zbr[i][j] = r;
				zbr[i][j + 1] = r;
			}
		}
	}

	void line() {
		const int width = sqrt(size);
		for (int i = 1; i < size; ++i) {
			for (int j = 0; j < size; j += width) {
				int r = random();
				for (int k = 0; k < width; ++k) {
					zbr[i][j + k] = r;
				}
			}
		}
	}

	void block() {
		int js[4] = { 0, 2, 8, 10 };
		for (int i = 1; i < size; ++i) {
			for (int j = 0; j < 4; ++j) {
				int r = random();
				zbr[i][js[j]] = r; // zbr[number][place]
				zbr[i][js[j] + 1] = r;
				zbr[i][js[j] + 4] = r;
				zbr[i][js[j] + 4 + 1] = r;
			}
		}
	}

	void four24() {
		int one[6] = { 0, 1, 2, 5, 6, 7 };
		int two[6] = { 3, 4, 8, 9, 13, 14 };
		int three[6] = { 10, 11, 15, 16, 20, 21 };
		int four[6] = { 17, 18, 19, 22, 23, 24 };

		for (int i = 1; i < size; ++i) {
			int r = random();
			for (int j = 0; j < 6; ++j) {
				zbr[i][one[j]] = r; // zbr[number][place]
			}
			r = random();
			for (int j = 0; j < 6; ++j) {
				zbr[i][two[j]] = r; // zbr[number][place]
			}
			r = random();
			for (int j = 0; j < 6; ++j) {
				zbr[i][three[j]] = r; // zbr[number][place]
			}
			r = random();
			for (int j = 0; j < 6; ++j) {
				zbr[i][four[j]] = r; // zbr[number][place]
			}
			zbr[i][12] = 0;
		}
	}

	void two() {
		for (int i = 1; i < size; ++i) {
			for (int two = 0; two < 1; ++two) {
				int r = random();
				for (int j = 0; j < size / 2; ++j) {
					zbr[i][j + two * size / 2] = r;
				}
			}
		}
	}

	// This abstraction is divided by the position of tile 1, 2 and 3.
	// Other tiles do not matter.
	void abstraction() {
		for (int i = 1; i < size; ++i) {
			if (1 <= i && i <= 3) {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = random();
				}
			} else {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = 0;
				}
			}
		}
	}

	// This abstraction is divided by the position of tile 1, 2, 3 and 4.
	// Other tiles do not matter.
	void fourabstraction() {
		for (int i = 1; i < size; ++i) {
			if (1 <= i && i <= 4) {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = random();
				}
			} else {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = 0;
				}
			}
		}
	}

	void far_tiles_abstraction() {
		for (int i = 1; i < size; ++i) {
			if (i == 1 || i == 7 || i == 12) {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = random();
				}
			} else {
				for (int j = 0; j < size; ++j) {
					zbr[i][j] = 0;
				}
			}
		}
	}

	int mdist(int number, int place) const {
		int width = 4; // Hard coding
		int row = number / width, col = number % width;
		int grow = place / width, gcol = place % width;
		int sum = std::abs(gcol - col) + std::abs(grow - row);
	}

// The method to return zobrist value for the very first node.
	unsigned int hash(const char* const board) const {
		unsigned char h = '\0';
		for (int i = 0; i < 16; ++i) {
			h = (h ^ zbr[board[i]][i]);
		}
		return h;
	}

	unsigned int random() {
		return dis(gen) + INT_MAX;
	}

	void dump_table() {
		for (int i = 0; i < size; ++i) {
			for (int j = 0; j < size; ++j) {
				printf("(%d, %d) = %d\n", i, j, zbr[i][j]);
			}
		}
	}

	int tnum;
	unsigned int zbr[size][size];
// inc_zbr is the incremental XOR value for zobrist hash function.
// The value to XOR when the number moved from a to b is
// inc_zbr[number][a][b] or inc_zbr[number][b][a]
	unsigned int inc_zbr[size][size][size];

	std::random_device rd;
	std::mt19937 gen;
	std::uniform_int_distribution<> dis;
};

#endif /* ZOBRIST_H_ */
