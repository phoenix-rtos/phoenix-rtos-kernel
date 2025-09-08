/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Scheduling contexts
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/hal.h"
#include "lib/lib.h"

#include "sc.h"
#include "threads.h"


/*
 * The code here is supposed to maintain two invariants:
 * 1. A blocked thread donates its best SC to the thread it is blocked on
 * 2. everyone else runs their own best SC
 */


sched_context_t *_sc_best(thread_t *t)
{
	/* TODO: optimize */
	sched_context_t *best = t->scOwn;
	sched_context_t *sc = t->scDonated;

	if (sc != NULL) {
		do {
			if (sc->priority < best->priority) {
				best = sc;
			}
			sc = sc->dnext;
		} while (sc != t->scDonated);
	}

	return best;
}


void _sc_recalculate(thread_t *t)
{
	_threads_setActiveSc(t, _sc_best(t));
}


void _sc_blockOnRecv(thread_t *t, thread_t *recv)
{
	_thread_setBlocked(t, WAIT_RECV, recv);
	LIST_ADD_EX(&recv->boosters, t, boostNext, boostPrev);
}


static void _sc_unblock(thread_t *t)
{
	thread_t *recv = _thread_waitingRecv(t);

	if (recv != NULL) {
		LIST_REMOVE_EX(&recv->boosters, t, boostNext, boostPrev);
	}

	/*
	 * WAIT_CALL cleared by _proc_clearReplyChain(), which has to unlink the
	 * callee's reply list atomically.
	 */
	_thread_clearBlocked(t, WAIT_RECV);
	_thread_clearBlocked(t, WAIT_LOCK);
}


thread_t *_sc_blockedOn(thread_t *t)
{
	switch (t->blockedKind) {
		case WAIT_LOCK:
			LIB_ASSERT_ALWAYS(t->blockedOn != NULL, "tid %d: WAIT_LOCK with no lock", proc_getTid(t));
			return ((lock_t *)t->blockedOn)->owner;

		case WAIT_RECV:
		case WAIT_CALL:
			LIB_ASSERT_ALWAYS(t->blockedOn != NULL, "tid %d: WAIT kind %u with no target",
					proc_getTid(t), t->blockedKind);
			return (thread_t *)t->blockedOn;

		default:
			return NULL;
	}
}


/* Moves sc one step and makes the donor unschedulable. */
static void _sc_push(thread_t *from, thread_t *to, sched_context_t *sc)
{
	LIB_ASSERT(sc != NULL, "donating a NULL SC");
	LIB_ASSERT(sc->t == from, "donating an SC that from doesn't hold");
	LIB_ASSERT(sc != to->scOwn, "donating an SC back to the thread that owns it");

	if (sc != from->scOwn) {
		/* scDonated holds SCs donated to us; our own SC is never on it */
		LIST_REMOVE_EX(&from->scDonated, sc, dnext, dprev);
	}

	_threads_setActiveSc(from, NULL);
	from->scPushed = sc;
	sc->t = to;

	LIST_ADD_EX(&to->scDonated, sc, dnext, dprev);
}


/* Takes back the SC donor pushed to t, however deep into the the chain it got. */
static void _sc_pop(thread_t *donor, thread_t *t)
{
	sched_context_t *sc = donor->scPushed;
	thread_t *holder;
	size_t depth;

	LIB_ASSERT(sc != NULL, "popping an SC that nobody pushed");

	holder = sc->t;
	LIB_ASSERT(sc != holder->scOwn, "popping an SC from its own thread");

	for (depth = 0; (t != holder) && (depth < BWI_MAX_CHAIN_DEPTH); depth++) {
		LIB_ASSERT((t != NULL) && (t->scPushed == sc), "SC pushed off the chain");
		t->scPushed = NULL;
		t = _sc_blockedOn(t);
	}

	LIB_ASSERT(t == holder, "SC pushed off the chain");

	LIST_REMOVE_EX(&holder->scDonated, sc, dnext, dprev);

	sc->t = donor;
	donor->scPushed = NULL;

	if (sc != donor->scOwn) {
		LIST_ADD_EX(&donor->scDonated, sc, dnext, dprev);
	}
}


/* 1 if t is transitively blocked on target, so donating to t on target's behalf would close a cycle. */
int _sc_reaches(thread_t *t, const thread_t *target)
{
	unsigned int depth;

	for (depth = 0; (depth < BWI_MAX_CHAIN_DEPTH) && (t != NULL); depth++) {
		if (t == target) {
			return 1;
		}

		t = _sc_blockedOn(t);
	}

	/* a chain too long to walk is refused rather than risked */
	return (t != NULL) ? 1 : 0;
}


/* Restores invariant 1 from t to the end of its chain. */
static void _sc_propagate(thread_t *t)
{
	thread_t *next;
	size_t depth;

	for (depth = 0; (depth < BWI_MAX_CHAIN_DEPTH) && (t != NULL); depth++) {
		next = _sc_blockedOn(t);

		if (t->scPushed != NULL) {
			_sc_pop(t, next);
		}

		if (next == NULL) {
			_sc_recalculate(t);
		}
		else {
			_sc_push(t, next, _sc_best(t));
		}

		t = next;
	}

	LIB_ASSERT(t == NULL, "PI chain too deep (cycle?)");
}


/* Takes back the SC t donated (undoes the donation). Does not dequeue t. */
void _sc_reclaim(thread_t *t)
{
	thread_t *next;

	if (t->scPushed == NULL) {
		/* donated nothing: not blocked on anyone, or waiting for a message */
		_sc_unblock(t);
		return;
	}

	next = _sc_blockedOn(t);
	_sc_pop(t, next);

	_sc_unblock(t);

	_sc_recalculate(t);
	_sc_propagate(next);
}


void _sc_donate(thread_t *from, thread_t *to, sched_context_t *sc)
{
	_sc_push(from, to, sc);
	_sc_propagate(to);
}


/* to must no longer be blocked on anyone. */
void _sc_return(thread_t *from, thread_t *to)
{
	LIB_ASSERT((to->scPushed != NULL) && (to->scPushed->t == from), "returning an SC that from doesn't hold");
	LIB_ASSERT(_thread_called(to) == NULL, "_sc_return but called not cleared?");

	_sc_pop(to, from);

	_sc_recalculate(to);
	_sc_propagate(from);

	LIB_ASSERT(to->scActive != NULL, "returned an SC to an unschedulable thread");
	LIB_ASSERT(from->scActive != NULL, "returning an SC left from unschedulable");
}


/* WARN: pushed SCs on queue must be on from->scDonated list */
void _sc_migrate(thread_t *from, thread_t *queue, thread_t *to)
{
	thread_t *t = queue->qnext;
	sched_context_t *sc;

	while (t != queue) {
		sc = t->scPushed;

		LIB_ASSERT(sc->t == from, "migrating SC not held by from");
		LIB_ASSERT(sc != to->scOwn, "migrating an SC to the thread that owns it");

		LIST_REMOVE_EX(&from->scDonated, sc, dnext, dprev);
		sc->t = to;
		LIST_ADD_EX(&to->scDonated, sc, dnext, dprev);

		t = t->qnext;
	}
}
