/**
 * Copyright (c) 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD+Patents license found in the
 * LICENSE file in the root directory of this source tree.
 */


/* Copyright 2004-present Facebook. All Rights Reserved.
 *
 * Hamming distances. The binary vector dimensionality should be a multiple
 * of 64, as the elementary operations operate on words. If you really need
 * other sizes, just pad with 0s (this is done by function fvecs2bitvecs).
 *
 * User-defined type hamdis_t is used for distances because at this time
 * it is still uncler clear how we will need to balance
 * - flexibility in vector size (may need 16- or even 8-bit vectors)
 * - memory usage
 * - cache-misses when dealing with large volumes of data (fewer bits is better)
 *
 * hamdis_t should optimally be compatibe with one of the Torch Storage
 * (Byte,Short,Long) and therefore should be signed for 2-bytes and 4-bytes.
 */

#ifndef FAISS_hamming_h
#define FAISS_hamming_h


#include <stdint.h>

#include "Heap.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

/* The Hamming distance type should be exportable to Lua Tensor, which
   excludes most unsigned type */
typedef int32_t hamdis_t;

namespace faiss {

inline int popcount64(uint64_t x) {
#ifdef _MSC_VER
	return (int)__popcnt64(x);
#else
    return __builtin_popcountl(x);
#endif
}


/** Compute a set of Hamming distances between na and nb binary vectors
 *
 * @param  a             size na * nbytespercode
 * @param  b             size nb * nbytespercode
 * @param  nbytespercode should be multiple of 8
 * @param  dis           output distances, size na * nb
 */
void hammings (
        const uint8_t * a,
        const uint8_t * b,
        size_t na, size_t nb,
        size_t nbytespercode,
        hamdis_t * dis);

void bitvec_print (const uint8_t * b, size_t d);


/* Functions for casting vectors of regular types to compact bits.
   They assume proper allocation done beforehand, meaning that b
   should be be able to receive as many bits as x may produce.  */

/* Makes an array of bits from the signs of a float array. The length
   of the output array b is rounded up to byte size (allocate
   accordingly) */
void fvecs2bitvecs (
        const float * x,
        uint8_t * b,
        size_t d,
        size_t n);


void fvec2bitvec (const float * x, uint8_t * b, size_t d);



/** Return the k smallest Hamming distances for a set of binary query vectors
 * @param a       queries, size ha->nh * ncodes
 * @param b       database, size nb * ncodes
 * @param nb      number of database vectors
 * @param ncodes  size of the binary codes (bytes)
 * @param ordered if != 0: order the results by decreasing distance
 *                (may be bottleneck for k/n > 0.01) */
void hammings_knn (
        int_maxheap_array_t * ha,
        const uint8_t * a,
        const uint8_t * b,
        size_t nb,
        size_t ncodes,
        int ordered);

/* The core function, without initialization and no re-ordering */
void hammings_knn_core (
        int_maxheap_array_t * ha,
        const uint8_t * a,
        const uint8_t * b,
        size_t nb,
        size_t ncodes);


/* Counting the number of matches or of cross-matches (without returning them)
   For use with function that assume pre-allocated memory */
void hamming_count_thres (
        const uint8_t * bs1,
        const uint8_t * bs2,
        size_t n1,
        size_t n2,
        hamdis_t ht,
        size_t ncodes,
        size_t * nptr);

/* Return all Hamming distances/index passing a thres. Pre-allocation of output
   is required. Use hamming_count_thres to determine the proper size. */
size_t match_hamming_thres (
        const uint8_t * bs1,
        const uint8_t * bs2,
        size_t n1,
        size_t n2,
        hamdis_t ht,
        size_t ncodes,
        long * idx,
        hamdis_t * dis);

/* Cross-matching in a set of vectors */
void crosshamming_count_thres (
        const uint8_t * dbs,
        size_t n,
        hamdis_t ht,
        size_t ncodes,
        size_t * nptr);


/* compute the Hamming distances between two codewords of nwords*64 bits */
hamdis_t hamming (
        const uint64_t * bs1,
        const uint64_t * bs2,
        size_t nwords);




/******************************************************************
 * The HammingComputer series of classes compares a single code of
 * size 4 to 32 to incoming codes. They are intended for use as a
 * template class where it would be inefficient to switch on the code
 * size in the inner loop. Hopefully the compiler will inline the
 * hamming() functions and put the a0, a1, ... in registers.
 ******************************************************************/


struct HammingComputer4 {
    uint32_t a0;

    HammingComputer4 (const uint8_t *a, int code_size) {
        assert (code_size == 4);
        a0 = *(uint32_t *)a;
    }

    inline int hamming (const uint8_t *b) const {
        return popcount64 (*(uint32_t *)b ^ a0);
    }

};

struct HammingComputer8 {
    uint64_t a0;

    HammingComputer8 (const uint8_t *a, int code_size) {
        assert (code_size == 8);
        a0 = *(uint64_t *)a;
    }

    inline int hamming (const uint8_t *b) const {
        return popcount64 (*(uint64_t *)b ^ a0);
    }

};


struct HammingComputer16 {
    uint64_t a0, a1;
    HammingComputer16 (const uint8_t *a8, int code_size) {
        assert (code_size == 16);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return popcount64 (b[0] ^ a0) + popcount64 (b[1] ^ a1);
    }

};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244) //conversion from uint64 -> uint32 on a2
#endif
// when applied to an array, 1/2 of the 64-bit accesses are unaligned.
// This incurs a penalty of ~10% wrt. fully aligned accesses.
struct HammingComputer20 {
    uint64_t a0, a1;
    uint32_t a2;

    HammingComputer20 (const uint8_t *a8, int code_size) {
        assert (code_size == 20);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1]; a2 = a[2];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return popcount64 (b[0] ^ a0) + popcount64 (b[1] ^ a1) +
            popcount64 (*(uint32_t*)(b + 2) ^ a2);
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct HammingComputer32 {
    uint64_t a0, a1, a2, a3;

    HammingComputer32 (const uint8_t *a8, int code_size) {
        assert (code_size == 32);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1]; a2 = a[2]; a3 = a[3];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return popcount64 (b[0] ^ a0) + popcount64 (b[1] ^ a1) +
            popcount64 (b[2] ^ a2) + popcount64 (b[3] ^ a3);
    }

};

struct HammingComputer64 {
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;

    HammingComputer64 (const uint8_t *a8, int code_size) {
        assert (code_size == 64);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1]; a2 = a[2]; a3 = a[3];
        a4 = a[4]; a5 = a[5]; a6 = a[6]; a7 = a[7];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return popcount64 (b[0] ^ a0) + popcount64 (b[1] ^ a1) +
            popcount64 (b[2] ^ a2) + popcount64 (b[3] ^ a3) +
            popcount64 (b[4] ^ a4) + popcount64 (b[5] ^ a5) +
            popcount64 (b[6] ^ a6) + popcount64 (b[7] ^ a7);
    }

};

struct HammingComputerDefault {
    const uint8_t *a;
    int n;

    HammingComputerDefault (const uint8_t *a8, int code_size) {
        a =  a8;
        n = code_size;
    }

    int hamming (const uint8_t *b8) const {
        int accu = 0;
        for (int i = 0; i < n; i++)
            accu += popcount64 (a[i] ^ b8[i]);
        return accu;
    }

};


struct HammingComputerM8 {
    const uint64_t *a;
    int n;

    HammingComputerM8 (const uint8_t *a8, int code_size) {
        assert (code_size % 8 == 0);
        a =  (uint64_t *)a8;
        n = code_size / 8;
    }

    int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        int accu = 0;
        for (int i = 0; i < n; i++)
            accu += popcount64 (a[i] ^ b[i]);
        return accu;
    }

};

// very inefficient...
struct HammingComputerM4 {
    const uint32_t *a;
    int n;

    HammingComputerM4 (const uint8_t *a4, int code_size) {
        assert (code_size % 4 == 0);
        a =  (uint32_t *)a4;
        n = code_size / 4;
    }
    int hamming (const uint8_t *b8) const {
        const uint32_t *b = (uint32_t *)b8;
        int accu = 0;
        for (int i = 0; i < n; i++)
             accu += popcount64 (a[i] ^ b[i]);
        return accu;
    }

};

/***************************************************************************
 * Equivalence with a template class when code size is known at compile time
 **************************************************************************/

// default template
template<int CODE_SIZE>
struct HammingComputer: HammingComputerM8 {
    HammingComputer (const uint8_t *a, int code_size):
    HammingComputerM8(a, code_size) {}
};

#define SPECIALIZED_HC(CODE_SIZE)                     \
    template<> struct HammingComputer<CODE_SIZE>:     \
            HammingComputer ## CODE_SIZE {            \
        HammingComputer (const uint8_t *a):           \
        HammingComputer ## CODE_SIZE(a, CODE_SIZE) {} \
    }

SPECIALIZED_HC(4);
SPECIALIZED_HC(8);
SPECIALIZED_HC(16);
SPECIALIZED_HC(20);
SPECIALIZED_HC(32);
SPECIALIZED_HC(64);

#undef SPECIALIZED_HC


/***************************************************************************
 * generalized Hamming = number of bytes that are different between
 * two codes.
 ***************************************************************************/


inline int generalized_hamming_64 (uint64_t a) {
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a &= 0x0101010101010101UL;
    return popcount64 (a);
}


struct GenHammingComputer8 {
    uint64_t a0;

    GenHammingComputer8 (const uint8_t *a, int code_size) {
        assert (code_size == 8);
        a0 = *(uint64_t *)a;
    }

    inline int hamming (const uint8_t *b) const {
        return generalized_hamming_64 (*(uint64_t *)b ^ a0);
    }

};


struct GenHammingComputer16 {
    uint64_t a0, a1;
    GenHammingComputer16 (const uint8_t *a8, int code_size) {
        assert (code_size == 16);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return generalized_hamming_64 (b[0] ^ a0) +
            generalized_hamming_64 (b[1] ^ a1);
    }

};

struct GenHammingComputer32 {
    uint64_t a0, a1, a2, a3;

    GenHammingComputer32 (const uint8_t *a8, int code_size) {
        assert (code_size == 32);
        const uint64_t *a = (uint64_t *)a8;
        a0 = a[0]; a1 = a[1]; a2 = a[2]; a3 = a[3];
    }

    inline int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        return generalized_hamming_64 (b[0] ^ a0) +
            generalized_hamming_64 (b[1] ^ a1) +
            generalized_hamming_64 (b[2] ^ a2) +
            generalized_hamming_64 (b[3] ^ a3);
    }

};

struct GenHammingComputerM8 {
    const uint64_t *a;
    int n;

    GenHammingComputerM8 (const uint8_t *a8, int code_size) {
        assert (code_size % 8 == 0);
        a =  (uint64_t *)a8;
        n = code_size / 8;
    }

    int hamming (const uint8_t *b8) const {
        const uint64_t *b = (uint64_t *)b8;
        int accu = 0;
        for (int i = 0; i < n; i++)
            accu += generalized_hamming_64 (a[i] ^ b[i]);
        return accu;
    }

};


/** generalized Hamming distances (= count number of code bytes that
    are the same) */
void generalized_hammings_knn (
        int_maxheap_array_t * ha,
        const uint8_t * a,
        const uint8_t * b,
        size_t nb,
        size_t code_size,
        int ordered = true);




} // namespace faiss




#endif /* FAISS_hamming_h */
