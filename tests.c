/*
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2020 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 * This file contains the functions for the actual tests, called from the
 * main routine in memtester.c.  See other comments in that file.
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>

#include "types.h"
#include "sizes.h"
#include "memtester.h"
#include "output.h"
#include "tests.h"

#define ONE 0x00000001L

union {
    unsigned char bytes[UL_LEN/8];
    ul val;
} mword8;

union {
    unsigned short u16s[UL_LEN/16];
    ul val;
} mword16;

/* Function definitions. */

int compare_regions(ulv *bufa, ulv *bufb, size_t count) {
    int r = 0;
    size_t i;
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    off_t physaddr;

    for (i = 0; i < count; i++, p1++, p2++) {
        if (*p1 != *p2) {
            if (use_phys) {
                physaddr = physaddrbase + (i * sizeof(ul));
                fprintf(stderr,
                        "FAILURE: 0x%08lx != 0x%08lx at physical address "
                        "0x%08lx.\n",
                        (ul) *p1, (ul) *p2, physaddr);
            } else {
                fprintf(stderr,
                        "FAILURE: 0x%08lx != 0x%08lx at offset 0x%08lx.\n",
                        (ul) *p1, (ul) *p2, (ul) (i * sizeof(ul)));
            }
            /* printf("Skipping to next test..."); */
            r = -1;
        }
    }
    return r;
}

int test_stuck_address(ulv *bufa, size_t count) {
    ulv *p1 = bufa;
    unsigned int j;
    size_t i;
    off_t physaddr;

    out_test_start();
    for (j = 0; j < 16; j++) {
        p1 = (ulv *) bufa;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            *p1 = ((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1);
            *p1++;
        }
        out_test_testing(j);
        p1 = (ulv *) bufa;
        for (i = 0; i < count; i++, p1++) {
            if (*p1 != (((j + i) % 2) == 0 ? (ul) p1 : ~((ul) p1))) {
                if (use_phys) {
                    physaddr = physaddrbase + (i * sizeof(ul));
                    fprintf(stderr,
                            "FAILURE: possible bad address line at physical "
                            "address 0x%08lx.\n",
                            physaddr);
                } else {
                    fprintf(stderr,
                            "FAILURE: possible bad address line at offset "
                            "0x%08lx.\n",
                            (ul) (i * sizeof(ul)));
                }
                printf("Skipping to next test...\n");
                fflush(stdout);
                return -1;
            }
        }
    }
    out_test_end();
    return 0;
}
struct test_stuck_address_args {
    unsigned long volatile *bufa;
    size_t count;
    int* result;
};
void* run_test_stuck_address_write_result(void *args) {
    struct test_stuck_address_args *p = (struct test_stuck_address_args *)args;
    *(p->result) = test_stuck_address(p->bufa, p->count);
}
int test_stuck_address_mt(unsigned long volatile *bufa, size_t count, int num_threads) {
    int result[1024];
    pthread_t thread_id[1024];
    size_t count_per_threads = count / num_threads;
    struct test_stuck_address_args args[1024];
    for(int i = 0; i < num_threads; i ++) {
        args[i].bufa = bufa + count_per_threads * i;
        args[i].count = count_per_threads;
        args[i].result = &result[i];
        pthread_create(&thread_id[i], NULL, &run_test_stuck_address_write_result, &args[i]);
    }

    for(int i = 0; i < num_threads; i ++) {
        pthread_join(thread_id[i], NULL);
    }
    for (int i = 0; i < num_threads; i ++) {
        if (result[i] != 0) {
            return result[i];
        }
    }
    return 0;
}

struct test_thread_args {
    test_func fp;
    unsigned long volatile *bufa;
    unsigned long volatile *bufb;
    size_t count;
    int* result;
};
void* run_test_write_result(void *args) {
    struct test_thread_args *p = (struct test_stuck_address_args *)args;
    *(p->result) = (p->fp)(p->bufa, p->bufb, p->count);
}
int run_test_mt(test_func fp, ulv *bufa, ulv *bufb, size_t count, int num_threads) {
    int result[1024];
    pthread_t thread_id[1024];
    size_t count_per_threads = count / num_threads;
    struct test_thread_args args[1024];
    for(int i = 0; i < num_threads; i ++) {
        args[i].fp = fp;
        args[i].bufa = bufa + count_per_threads * i;
        args[i].bufb = bufb + count_per_threads * i;
        args[i].count = count_per_threads;
        args[i].result = &result[i];
        pthread_create(&thread_id[i], NULL, &run_test_write_result, &args[i]);
    }

    for(int i = 0; i < num_threads; i ++) {
        pthread_join(thread_id[i], NULL);
    }
    for (int i = 0; i < num_threads; i ++) {
        if (result[i] != 0) {
            return result[i];
        }
    }
    return 0;
}

int test_random_value(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;

    out_wheel_start();
    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = rand_ul();
        out_wheel_advance(i);
    }
    out_wheel_end();
    return compare_regions(bufa, bufb, count);
}

int test_xor_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ ^= q;
        *p2++ ^= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_sub_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ -= q;
        *p2++ -= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_mul_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ *= q;
        *p2++ *= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_div_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        if (!q) {
            q++;
        }
        *p1++ /= q;
        *p2++ /= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_or_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ |= q;
        *p2++ |= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_and_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ &= q;
        *p2++ &= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_seqinc_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = (i + q);
    }
    return compare_regions(bufa, bufb, count);
}

int test_solidbits_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    out_test_start();
    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? UL_ONEBITS : 0;
        out_test_setting(j);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_checkerboard_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    out_test_start();
    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? CHECKERBOARD1 : CHECKERBOARD2;
        out_test_setting(j);
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_blockseq_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < 256; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (ul) UL_BYTE(j);
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_walkbits0_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = ONE << j;
            } else { /* Walk it back down. */
                *p1++ = *p2++ = ONE << (UL_LEN * 2 - j - 1);
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_walkbits1_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << j);
            } else { /* Walk it back down. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << (UL_LEN * 2 - j - 1));
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_bitspread_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    out_test_start();
    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *) bufa;
        p2 = (ulv *) bufb;
        out_test_setting(j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << j) | (ONE << (j + 2))
                    : UL_ONEBITS ^ ((ONE << j)
                                    | (ONE << (j + 2)));
            } else { /* Walk it back down. */
                *p1++ = *p2++ = (i % 2 == 0)
                    ? (ONE << (UL_LEN * 2 - 1 - j)) | (ONE << (UL_LEN * 2 + 1 - j))
                    : UL_ONEBITS ^ (ONE << (UL_LEN * 2 - 1 - j)
                                    | (ONE << (UL_LEN * 2 + 1 - j)));
            }
        }
        out_test_testing(j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_test_end();
    return 0;
}

int test_bitflip_comparison(ulv *bufa, ulv *bufb, size_t count) {
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j, k;
    ul q;
    size_t i;

    out_test_start();
    for (k = 0; k < UL_LEN; k++) {
        q = ONE << k;
        for (j = 0; j < 8; j++) {
            q = ~q;
            out_test_setting(k * 8 + j);
            p1 = (ulv *) bufa;
            p2 = (ulv *) bufb;
            for (i = 0; i < count; i++) {
                *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
            }
            out_test_testing(k * 8 + j);
            fflush(stdout);
            if (compare_regions(bufa, bufb, count)) {
                return -1;
            }
        }
    }
    out_test_end();
    return 0;
}

#ifdef TEST_NARROW_WRITES
int test_8bit_wide_random(ulv* bufa, ulv* bufb, size_t count) {
    u8v *p1, *t;
    ulv *p2;
    int attempt;
    unsigned int b;
    size_t i;

    out_wheel_start();
    for (attempt = 0; attempt < 2;  attempt++) {
        if (attempt & 1) {
            p1 = (u8v *) bufa;
            p2 = bufb;
        } else {
            p1 = (u8v *) bufb;
            p2 = bufa;
        }
        for (i = 0; i < count; i++) {
            t = mword8.bytes;
            *p2++ = mword8.val = rand_ul();
            for (b=0; b < UL_LEN/8; b++) {
                *p1++ = *t++;
            }
            out_wheel_advance(i);
        }
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_wheel_end();
    return 0;
}

int test_16bit_wide_random(ulv* bufa, ulv* bufb, size_t count) {
    u16v *p1, *t;
    ulv *p2;
    int attempt;
    unsigned int b;
    size_t i;

    out_wheel_start();
    for (attempt = 0; attempt < 2; attempt++) {
        if (attempt & 1) {
            p1 = (u16v *) bufa;
            p2 = bufb;
        } else {
            p1 = (u16v *) bufb;
            p2 = bufa;
        }
        for (i = 0; i < count; i++) {
            t = mword16.u16s;
            *p2++ = mword16.val = rand_ul();
            for (b = 0; b < UL_LEN/16; b++) {
                *p1++ = *t++;
            }
            out_wheel_advance(i);
        }
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    out_wheel_end();
    return 0;
}
#endif
