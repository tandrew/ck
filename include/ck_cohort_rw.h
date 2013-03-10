/*
 * Copyright 2013 Samy Al Bahra.
 * Copyright 2013 Brendon Scheinman.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_COHORT_RW_H
#define _CK_COHORT_RW_H

/*
 * This is an implementation of NUMA-aware reader-writer locks as described in:
 *     Calciu, I.; Dice, D.; Lev, Y.; Luchangco, V.; Marathe, V.; and Shavit, N. 2013.
 *     NUMA-Aware Reader-Writer Locks
 */

#include <ck_cc.h>
#include <ck_pr.h>
#include <stddef.h>
#include <ck_cohort.h>

#define CK_COHORT_RW_NAME(N) ck_cohort_rw_##N
#define CK_COHORT_RW_INSTANCE(N) struct CK_COHORT_RW_NAME(N)
#define CK_COHORT_RW_INIT(N, RW, WL) ck_cohort_rw_##N##_init(RW, WL)
#define CK_COHORT_RW_READ_LOCK(N, RW, C, GC, LC) ck_cohort_rw_##N##_read_lock(RW, C, GC, LC)
#define CK_COHORT_RW_READ_UNLOCK(N, RW) ck_cohort_rw_##N##_read_unlock(RW)
#define CK_COHORT_RW_WRITE_LOCK(N, RW, C, GC, LC) ck_cohort_rw_##N##_write_lock(RW, C, GC, LC)
#define CK_COHORT_RW_WRITE_UNLOCK(N, RW, C, GC, LC) ck_cohort_rw_##N##_write_unlock(RW, C, GC, LC)
#define CK_COHORT_RW_DEFAULT_WAIT_LIMIT 1000

#define CK_COHORT_RW_PROTOTYPE(N)								\
	CK_COHORT_RW_INSTANCE(N) {								\
		CK_COHORT_INSTANCE(N) *cohort;							\
		unsigned int read_counter;							\
		unsigned int write_barrier;							\
		unsigned int wait_limit;							\
	};											\
												\
	CK_CC_INLINE static void								\
	ck_cohort_rw_##N##_init(CK_COHORT_RW_INSTANCE(N) *rw_cohort,				\
	    unsigned int wait_limit)								\
	{											\
		rw_cohort->read_counter = 0;							\
		rw_cohort->write_barrier = 0;							\
		rw_cohort->wait_limit = wait_limit;						\
		ck_pr_barrier();								\
		return;										\
	}											\
												\
	CK_CC_INLINE static void								\
	ck_cohort_rw_##N##_write_lock(CK_COHORT_RW_INSTANCE(N) *rw_cohort,			\
	    CK_COHORT_INSTANCE(N) *cohort, void *global_context,				\
	    void *local_context)								\
	{											\
		while (ck_pr_load_uint(&rw_cohort->write_barrier) > 0) {			\
			ck_pr_stall();								\
		}										\
												\
		CK_COHORT_LOCK(N, cohort, global_context, local_context);			\
												\
		while (ck_pr_load_uint(&rw_cohort->read_counter) > 0) {				\
			ck_pr_stall();								\
		}										\
												\
		return;										\
	}											\
												\
	CK_CC_INLINE static void								\
	ck_cohort_rw_##N##_write_unlock(CK_COHORT_RW_INSTANCE(N) *rw_cohort,			\
	    CK_COHORT_INSTANCE(N) *cohort, void *global_context,				\
	    void *local_context)								\
	{											\
		(void)rw_cohort;								\
		CK_COHORT_UNLOCK(N, cohort, global_context, local_context);			\
	}											\
												\
	CK_CC_INLINE static void								\
	ck_cohort_rw_##N##_read_lock(CK_COHORT_RW_INSTANCE(N) *rw_cohort,			\
	    CK_COHORT_INSTANCE(N) *cohort, void *global_context,				\
	    void *local_context)								\
	{											\
		unsigned int wait_count = 0;							\
		bool raised = false;								\
	start:											\
		ck_pr_inc_uint(&rw_cohort->read_counter);					\
		if (CK_COHORT_LOCKED(N, cohort, global_context, local_context) == true) {	\
			ck_pr_dec_uint(&rw_cohort->read_counter);				\
			while (CK_COHORT_LOCKED(N, cohort, global_context, local_context) == true) {\
				ck_pr_stall();							\
				if (++wait_count > rw_cohort->wait_limit && raised == false) {	\
					ck_pr_inc_uint(&rw_cohort->write_barrier);		\
					raised = true;						\
				}								\
			}									\
			goto start;								\
		}										\
												\
		if (raised == true) {								\
			ck_pr_dec_uint(&rw_cohort->write_barrier);				\
		}										\
												\
		return;										\
	}											\
												\
	CK_CC_INLINE static void								\
	ck_cohort_rw_##N##_read_unlock(CK_COHORT_RW_INSTANCE(N) *cohort)			\
	{											\
		ck_pr_dec_uint(&cohort->read_counter);						\
	}

#define CK_COHORT_RW_INITIALIZER {								\
	.cohort = NULL,										\
	.read_counter = 0,									\
	.write_barrier = 0,									\
	.wait_limit = 0										\
}

#endif /* _CK_COHORT_RW_H */
