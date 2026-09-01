# BWI: Bandwidth Inheritance

BWI ("Bandwidth Inheritance") is the priority-inheritance (PI) mechanism used
by `PH_LOCK_PROTO_INHERIT` mutexes and by IPC (`msgSend`/`msgRecv`/`respond`)
in `proc/threads.c`. It replaced an older scheme (nicknamed "sha-like") that
walked the lock-ownership chain by hand, boosting each owner's raw
`thread->priority` field. BWI instead donates a **scheduling context**
(`sched_context_t`, "SC") from the blocked thread to whoever it's waiting on.
The recipient runs using the donor's SC, which carries the donor's priority,
for as long as it's the best SC available to it.

This document describes the mechanism end to end, including every corner
case that was wrong at some point during its development and had to be
fixed. If you're modifying this code, read the relevant corner-case section
before touching anything.

## 0. Academic background

### 0.1 Classic Priority Inheritance (Sha, Rajkumar & Lehoczky, 1990)

The original Priority Inheritance Protocol (PIP) and Priority Ceiling
Protocol (PCP) come from Sha, Rajkumar & Lehoczky, *"Priority Inheritance
Protocols: An Approach to Real-Time Synchronization"*, IEEE Transactions on
Computers, 1990. Under fixed-priority preemptive scheduling, when a
high-priority task `H` blocks on a resource held by a lower-priority task
`L`, `L` temporarily inherits `H`'s **priority** - a single scalar number -
until it releases the resource. This bounds priority inversion: basic PIP
bounds it to at most one lower-priority critical section per resource held
by tasks `H` can block on; PCP tightens that to one critical section total,
and also prevents deadlock and chained blocking. The bound is then folded
into classical response-time analysis as a blocking term `B_i`.

This is exactly what this kernel's *previous* ("sha-like") scheme did: walk
the chain of lock owners by hand and boost each one's raw
`thread->priority` field, one integer at a time. It is also what Linux's
`rt_mutex` PI-boosting (`task->prio`/`task->normal_prio`, used by
`PREEMPT_RT` for over a decade) does.

Classic PIP's bound is expressed in *priority* and *time*, and its analysis
assumes a fixed-priority, single global schedule. It has no notion of a
per-task *temporal budget* - which becomes a problem once tasks are
scheduled by **reservation** (each task gets a guaranteed share of the CPU,
enforced independently of what else is running) rather than by a single
global priority order.

### 0.2 Bandwidth Inheritance (Lamastra, Lipari & Abeni, 2001)

Reservation-based scheduling - e.g. a Constant Bandwidth Server (CBS) under
EDF, where each task/component gets a budget `Q` and period `P` forming a
"server" with bandwidth `Q/P` - exists to give **temporal isolation**:
tasks can be admitted, removed, or modified independently, because a
schedulability test only needs to check that the sum of bandwidths fits,
not re-analyze the whole system's worst-case execution times against each
other. Under EDF, "priority" is not even a fixed number - it's a deadline
that changes every job - so simply "boosting a priority" doesn't compose
with reservations at all: lending a blocked task's *deadline* to a lock
holder says nothing about whose *budget* the holder now runs on, and an
uncontrolled loan would let one component's blocking silently eat into
another's guaranteed bandwidth, destroying the isolation the whole
reservation scheme exists to provide.

Bandwidth Inheritance, proposed by Lamastra, Lipari & Abeni, *"A Bandwidth
Inheritance Algorithm for Real-Time Task Synchronization in Open Systems"*
(RTSS 2001), and extended to multiprocessors by Faggioli, Lipari &
Cucinotta, *"The Multiprocessor Bandwidth Inheritance Protocol"* (ECRTS
2010), fixes this by donating the **whole server** - budget and deadline
together, not a bare number - to the resource holder. The holder executes
*using the blocked task's reservation*: any time spent in the critical
section is charged to the budget the blocked task itself offered up by
blocking, never to the holder's own, unrelated reservation. This preserves
composability: a task's schedulability guarantee only depends on the
(bounded, small) set of tasks it directly shares resources with, the same
property PCP gives for priorities, but expressed in bandwidth terms so it
survives reservation-based admission control in open, dynamically
composed systems.

This is precisely why this kernel's mechanism donates a `sched_context_t`
- a schedulable *entity* - end to end through arbitrary chains of locks
and IPC calls (Section 5, Section 12), rather than a raw priority integer:
the SC is the unit that the scheduler and ready queues actually operate
on, so handing the entire object across a chain keeps it correctly linked
into ready queues, correctly accounted for CPU-time bookkeeping, and
correctly identified as "the thing to run" everywhere the scheduler looks
- exactly analogous to handing over a whole server, not just its number.

### 0.3 Why this kernel doesn't get BWI's formal guarantee, and what to do about it

Academic BWI's guarantee is a *bandwidth-preservation* theorem: a task's
schedulability is only affected by the (small, bounded) set of tasks it
directly shares resources with, because blocking is always paid for out of
a budget that was already accounted for in admission control. That
guarantee is a direct consequence of the CBS/EDF machinery it's proven
against - budgets, periods, and a bandwidth-based admission test - none of
which exists in this kernel. Concretely, here's what's missing and why it
matters:

- **No budget, so nothing is "charged".** `_sc_donateAt()` moves a
  `priority`/`priorityBase` pair around, not a `(runtime, deadline)` pair.
  When a lock holder runs under a donor's boosted priority, no accounting
  records how much CPU time that donor "spent" via the loan. Two
  consequences: (1) there is no way to detect or bound a holder that
  overruns while boosted - it just keeps running at the donated priority
  until it releases the lock, however long that takes; (2) there is no
  way to answer "how much of task X's guaranteed CPU share did blocking
  actually consume", which is the whole quantity academic BWI's proof
  bounds.
- **No admission control, so there's no bandwidth to preserve.** BWI's
  isolation claim presupposes a system where `sum(Q_i/P_i) <= 1` was
  checked at admission time. This kernel has fixed priority levels
  (`NPRIOS`) with no per-thread budget or period, so there is no
  utilization bound being protected in the first place - "preserve
  bandwidth isolation" isn't a meaningful statement to make about it yet.
- **The bound this kernel *does* have is the PIP/PCP one, not BWI's.**
  What's proven informally by this design (and enforced at runtime by
  `BWI_MAX_CHAIN_DEPTH`, Section 5.3) is a chain-depth-bounded priority
  inversion, same flavor as classic PIP/PCP (Section 0.1) - not a
  bandwidth bound. That's a legitimate, useful guarantee; it just isn't
  the one the protocol's name suggests.

**How to close the gap, if a real bandwidth guarantee is ever needed:**

1. **Give threads a budget, not just a priority.** Add `runtime`/`period`
   (or `runtime`/`deadline`) fields to `sched_context_t` alongside
   `priority`, and switch the ready queue from `NPRIOS` fixed levels to an
   EDF (or similar deadline-ordered) queue. This is the prerequisite for
   everything else - without a budget concept, "charge the blocking time
   to someone's reservation" has nothing to charge.
2. **Turn `_sc_donateAt()`'s priority boost into a real budget loan.**
   Instead of `sc->priority = min(sc->priority, from->priority)`, the
   donated SC's `(runtime, deadline)` pair becomes what the holder
   consumes from while it holds the lock, exactly as in Lamastra/Lipari/
   Abeni's algorithm. `_sc_return()`'s "restore to priorityBase" step
   becomes "hand back whatever runtime is left, at the original
   deadline".
3. **Add admission control.** Reject `proc_threadCreate()`/reservation
   requests that would push `sum(Q_i/P_i)` over the schedulable bound,
   the same way CBS-based systems do. Without this step, step 1-2 give
   you EDF scheduling with budget-based PI, but not the isolation
   *guarantee* - that specifically comes from bounding total admitted
   bandwidth up front.
4. **Re-derive the blocking bound for this kernel's actual chain shape.**
   The multiprocessor BWI paper (Faggioli, Lipari & Cucinotta, 2010) is
   the closer reference once locks can be taken across cores; the
   original single-processor paper is closer to this kernel's current
   single-core deployment. Either way, the bound must be re-derived
   against *this* implementation's chain-depth limit and IPC-chain
   shape (Section 6, Section 12) - don't assume the paper's bound
   transfers without re-checking that this code's invariants (donor
   stacking, migration, `waitingOn` clearing) match what the proof
   assumes.
5. **Decide if it's worth it.** Budget-based scheduling is a substantial
   change for a microkernel whose current clients assume fixed-priority
   scheduling. If the actual requirement is "bounded priority inversion",
   what's implemented today already provides that (Section 0.1's bound,
   via chain-depth limiting) without the cost of an EDF rewrite. Only
   pursue 1-4 if bandwidth isolation between independently-developed
   components is an actual product requirement, not because the name
   "Bandwidth Inheritance" implies you should already have it.

### 0.4 Linux Proxy Execution

Linux's historical `rt_mutex` PI-boosting is, like this kernel's old
"sha-like" scheme, structurally classic PIP: it boosts `task->prio`, which
only exists as a simple scalar for the `SCHED_FIFO`/`SCHED_RR`
fixed-priority classes. It never composed cleanly with `CFS`
(weighted fair-share, no single priority number) or `SCHED_DEADLINE`
(Linux's own CBS implementation, which - like academic BWI's target
systems - depends on per-task runtime/deadline budgets for its
admission-control guarantees).

**Proxy Execution**, developed for mainline Linux by Juri Lelli, Valentin
Schneider, Connor O'Brien, Peter Zijlstra and others (building on earlier
work by Watkins & Walker), generalizes the same core idea - inherit the
*scheduling entity*, not a number - to Linux's full scheduler stack. When a
task blocks on a mutex, the scheduler doesn't boost the owner's priority;
it walks the `blocked_on` chain to find the actual runnable task at the end
of it and executes *that* task's context using the blocked task's
scheduling parameters (vruntime/deadline/CPU affinity) as a "proxy",
including migrating the donated context across CPUs, which a full SMP
scheduler must support and this kernel's chain-relay does not attempt.

The relationship to BWI: for `SCHED_DEADLINE` tasks specifically, Proxy
Execution *is* Linux's implementation of Bandwidth Inheritance - a blocked
deadline task's server is what gets handed to the lock holder - generalized
by the proxy-execution mechanism to also work uniformly for `CFS` and
`SCHED_FIFO`/`RR` tasks through the same code path, rather than requiring a
separate PI scheme per scheduling class. This kernel's BWI is a much
smaller-scale relative of the same idea: single ready-queue array instead
of three cooperating scheduling classes, no cross-CPU migration, and
eagerly relayed at donate-time (Section 5) rather than lazily resolved by
walking a `blocked_on` graph at `schedule()` time - but the shift it makes
over the old "sha-like" scheme is the same shift Proxy Execution makes over
classic `rt_mutex` boosting: stop moving a number, start moving the thing
the scheduler actually runs.

## 1. Data structures

Each `thread_t` owns exactly one `sched_context_t`, `scOwn`, allocated at
creation and never freed until the thread is. Relevant fields:

**On `sched_context_t`:**
- `owner` - the thread this SC was originally allocated for. Set once at
  creation, never changes. Used only for `LIB_ASSERT` sanity checks.
- `t` - whoever currently, physically carries this SC (either running with
  it as their `scActive`, or holding it in their `scDonated` pool as a
  spare). Updated at every donate/return/migrate/reclaim.
- `donor` - whoever most recently forwarded this SC to `t`. This is **not**
  the SC's original owner in a chain longer than one hop - see Section 5.
- `priority` / `priorityBase` - the SC's effective and base priority. For
  `scOwn`, these start equal to the thread's own priority/base.

**On `thread_t`:**
- `scOwn` - the thread's own SC (see above).
- `scActive` - the SC currently powering this thread's execution, or `NULL`
  if the thread has donated its own active SC away and is "away" (see
  Section 3). This is what the scheduler actually runs threads with -
  `_sc_best()` picks the best of `scOwn` + `scDonated` and stores it here
  whenever it changes.
- `scDonated` - a circular list of SCs currently donated *to* this thread,
  linked via `dnext`/`dprev`. Does not include `scOwn`.
- `prevDonor` - one level of the donor stack (see Section 5).
- `waitingOn` (`lock_t*`) - the lock this thread donated its own SC to wait
  on, or `NULL`. Cleared as soon as the lock is granted, not when the
  thread next runs (Section 7).
- `relayed` (`sched_context_t*`) - the single SC this thread is currently
  forwarding up its own chain while away, or `NULL`. At most one at a time
  (see Section 5).

## 2. The simple case: one waiter, no chain

Thread `H` calls `proc_lockSet()` on a lock held by `L`. `_proc_lockTry()`
returns `-EBUSY`. `_proc_lockSet()` then donates:

```c
donated = current->scActive;       /* H's own scActive - normally H->scOwn */
current->waitingOn = lock;
_sc_donate(current, lock->owner, donated);   /* H -> L */
```

`_sc_donate()` (really `_sc_donateAt()`, see Section 5) does, for the
simple case:
1. Removes `sc` from `H`'s pool (no-op if `sc == H->scOwn`, since `scOwn`
   is never list-linked).
2. Boosts `sc->priority` to `H`'s priority if better.
3. `H->scActive = NULL; H->relayed = sc;` - `H` is now "away".
4. `sc->donor = H; sc->t = L;`
5. Links `sc` into `L->scDonated`.
6. `L` is not away (it's just running normally), so: `L->scActive =
   _sc_best(L)` picks the better of `L->scOwn` and `H`'s SC, and
   `_sc_updateEffPriority(L)` propagates the boost into `L->priority`.

`H` then enqueues on `lock->queue` and reschedules (`_proc_lockWaitWake()`).

When `L` unlocks (`_proc_lockUnlock()`), it hands the lock to the queue head
(`H`) and calls `_sc_return(L, H, _sc_ofDonor(L, H))`, which:
1. Removes `sc` (found by `donor == H`) from `L->scDonated`.
2. Resets `sc->priority = sc->priorityBase` (the boost was temporary).
3. Unstacks: `sc->donor = H->prevDonor` (`NULL` here - see Section 5).
4. `H->scActive = sc; H->state = READY; H->relayed = NULL;`
5. Recomputes `L->scActive = _sc_best(L)` now that `H`'s SC is gone.

## 3. "Away": the unifying concept across locks and IPC

A thread is **away** when it has donated its own currently-active SC
elsewhere and is waiting for something. `_sc_awayTarget(t)` is the single
place that answers "who is `t` deferring to right now":

```c
static thread_t *_sc_awayTarget(thread_t *t)
{
	if (t->waitingOn != NULL) return t->waitingOn->owner;   /* lock wait */
	if (t->called != NULL) return t->called;                /* IPC call */
	return NULL;                                             /* not away */
}
```

This unification is what makes chains through *mixed* lock/IPC blocking
work (Section 6): a thread nested-blocked on a lock while itself in the
middle of an IPC call is handled by exactly the same relay code either way.

**Corner case - passive IPC receivers are not "away".** A thread parked in
`msgRecv()` with no message pending (`t->passive == 1`) also has
`scActive == NULL` (set explicitly in `_becomePassive()`), but it is not
deferring to anyone - it's simply idle. `_sc_awayTarget()` correctly
returns `NULL` for it (neither `waitingOn` nor `called` is set), so a
donation lands directly via the "not away" path in `_sc_donateAt()`
(Section 4), exactly like the simple case in Section 2. Treating
`scActive == NULL` alone as "away" (an earlier, wrong version of this code
did) breaks msgSend to any idle server.

## 4. `_sc_donateAt()`: the one-hop primitive

```c
static void _sc_donateAt(thread_t *from, thread_t *to, sched_context_t *sc, unsigned int depth)
{
	/* remove sc from from's pool, boost it, mark from away, stack the donor */
	...
	if (_sc_awayTarget(to) == NULL) {
		to->scActive = _sc_best(to);
		_sc_updateEffPriority(to);
		return;
	}
	_sc_relay(to, depth + 1);   /* to is itself away - keep the chain going */
}
```

If `to` isn't away, the donation lands directly (Section 2/3). If `to`
*is* away (nested lock, or itself blocked on an IPC call), the donation
can't be used directly - `to` isn't running. `_sc_relay()` (Section 5) is
responsible for propagating it further.

`_sc_donate(from, to, sc)` is a thin wrapper calling `_sc_donateAt(from, to,
sc, 0)` - the public entry point always starts a fresh chain-depth count.

## 5. Chains: relay, reclaim, and the donor stack

### 5.1 Why a chain needs more than "forward it once"

Consider: `C` holds mutex1. `B` holds mutex2, blocked on mutex1 (donated
`B`'s own SC to `C`). `A` tries mutex2, held by `B`. Since `B` is away
(`_sc_awayTarget(B) == C`), `A`'s donation can't land on `B` directly - it
must continue on to `C`, because `C` is who actually needs to run faster
for `B` to make progress and eventually free mutex2 for `A`.

`_sc_relay(to, depth)` handles this:

```c
static void _sc_relay(thread_t *to, unsigned int depth)
{
	if (to->relayed != NULL) {
		_sc_reclaim(to, to->relayed, depth + 1);
		to->relayed = NULL;
	}
	next = _sc_awayTarget(to);
	best = _sc_best(to);          /* best of to's whole pool: scOwn + scDonated */
	to->relayed = best;
	_sc_donateAt(to, next, best, depth + 1);
}
```

Every time a new donation arrives at an away thread, `_sc_relay()`
unconditionally re-evaluates from scratch: reclaim whatever was previously
forwarded, recompute the true best across the *entire* pool (own SC plus
every accumulated donation), and forward that. This is deliberately
non-optimized (a "did the new arrival actually beat the old one" shortcut
was tried and removed) because correctness here matters far more than
avoiding a redundant reclaim/re-donate round trip on the rare hop.

### 5.2 The donor stack: why `sc->donor` isn't the original owner

When `_sc_relay()` forwards `B`'s SC through to `C` on `B`'s behalf, it
calls `_sc_donateAt(B, C, B_own, ...)`, which re-stacks:

```c
from->prevDonor = sc->donor;   /* save who donated it to `from` (here: B itself) */
sc->donor = from;              /* now shows the immediate relay point */
```

So once `B` has relayed it onward, `B_own->donor == B` (not the original
waiter, if this were a 3+ hop chain) and `B->prevDonor` remembers the
previous link. **`sc->donor` always reflects only the immediate hop, never
the ultimate original donor.** This is intentional: it's what lets
`_sc_return()`/`_sc_reclaim()` unwind exactly one hop at a time,
symmetrically to how it was built up.

### 5.3 `_sc_reclaim()`: unwinding an arbitrary number of hops

Reclaiming `sc` back onto a specific `owner` several hops upstream requires
walking the donor chain, not jumping straight to `sc->t` (the current
physical location) - because `sc->donor` at that location names the
*immediate* relay point, not `owner`:

```c
static void _sc_reclaim(thread_t *owner, sched_context_t *sc, unsigned int depth)
{
	if (sc->donor != owner) {
		_sc_reclaim(sc->donor, sc, depth + 1);   /* unwind one hop, recurse */
	}
	LIB_ASSERT(sc->donor == owner, ...);          /* now guaranteed true */

	holder = sc->t;
	/* remove sc from holder's scDonated/scActive, clear holder->relayed if
	 * it pointed at sc, give sc to owner, unstack sc->donor from
	 * owner->prevDonor, then let holder re-relay or recompute if needed */
}
```

**Corner case - a stale `holder->relayed` after reclaiming.** If `holder`
(the thread `sc` is being taken away from) was itself actively forwarding
`sc` further via `_sc_relay()`, `holder->relayed` still points at `sc`
after the object physically moves elsewhere. Left uncleared, the next
`_sc_relay(holder, ...)` call would try to reclaim an SC that `holder` no
longer has any claim to, corrupting an unrelated part of the chain. This is
why `_sc_reclaim()` explicitly clears `holder->relayed` when it matches.

**Corner case - cycle / excessive chain depth.** Both `_sc_relay()` and
`_sc_reclaim()` take a `depth` parameter, asserting
`depth < BWI_MAX_CHAIN_DEPTH` (8). A real lock-ordering cycle between two
threads (A holds L1 wants L2, B holds L2 wants L1 - a caller bug, not a
BWI bug) would otherwise recurse forever between donate/relay/reclaim. The
assert turns that into a clear panic instead of a stack overflow.

## 6. Multi-waiter locks

When a lock has several queued waiters and is handed to the head of the
queue, the *other* waiters' donated SCs - all still sitting in the old
owner's `scDonated`, since they all donated directly to whoever held the
lock at the time they blocked - must follow the lock to its new owner.
`_proc_lockUnlock()` does this with `_sc_migrate()`, a simple re-home
(no donor-stack change, unlike `_sc_return`):

```c
if (lock->queue->qnext != lock->queue) {   /* more than just the new owner queued */
    for (waiter in remaining queue, skipping the new owner)
        _sc_migrate(owner, lock->owner, _sc_ofDonor(owner, waiter));
    lock->owner->scActive = _sc_best(lock->owner);
    owner->scActive = _sc_best(owner);
}
```

**Corner case - ordering vs. `_readyAdd()`.** This migration must happen
*before* `_proc_threadDequeue(lock->owner)`, which links the new owner's
*current* `scActive` into the ready queue. If the migration ran afterward
and then reassigned `scActive` (because a migrated waiter's SC is better
than what `_sc_return()` alone picked), the ready queue would be left
pointing at a stale, orphaned SC object, and the next
`_proc_threadSetPriority()` on that thread would fail to find it there.
Because the new owner hasn't been dequeued from `lock->queue` yet at this
point, the waiter loop explicitly skips the queue head (the new owner
itself) rather than relying on dequeue having already happened.

**Corner case - the old owner's own `scActive` goes stale.** `_sc_return()`
picks the *old* owner's best SC from its *pre-migration* pool (it runs
before migration). If that pick happens to be exactly one of the SCs the
migration step then moves away, `owner->scActive` is left dangling on an SC
it no longer carries. `_proc_lockUnlock()` explicitly recomputes
`owner->scActive = _sc_best(owner)` after migration to fix this.

## 7. `waitingOn` must be cleared at grant time, not at wake time

When a lock is handed off, `_proc_lockUnlock()` sets `lock->owner`, calls
`_sc_return()`, and eventually `_proc_threadDequeue()` (marking the new
owner `READY`) - but the new owner doesn't necessarily run immediately; it
just becomes eligible to be scheduled. The *natural* place to clear
`new_owner->waitingOn` would seem to be when the new owner itself resumes
and notices it got the lock. That is wrong.

**Corner case - self-referential relay window.** Between being granted the
lock and actually resuming execution, if any *other* thread donates to this
lock (extremely plausible under contention - see the stress scenarios in
`test/proc.c`), `_sc_awayTarget(new_owner)` would still evaluate
`new_owner->waitingOn->owner` - which is now `new_owner` itself, since it
already owns that very lock. The donation would try to relay to the thread
itself, corrupting the chain. `_proc_lockUnlock()` therefore clears
`lock->owner->waitingOn = NULL` immediately as part of the hand-off, in the
same critical section, closing this window entirely.

## 8. Interruption while away

If a thread waiting (interruptibly) on a lock is force-woken by a signal or
process teardown, `_thread_interrupt()` must reclaim its donated SC before
`_proc_threadDequeue()` - which asserts `scActive != NULL`:

```c
if (t->waitingOn != NULL) {
    sched_context_t *sc = t->relayed;   /* exactly what t donated */
    thread_t *holder = sc->t;           /* wherever it currently, physically lives */
    _sc_return(holder, t, sc);
    t->waitingOn = NULL;
    _proc_threadSetPriority(holder, _proc_threadGetPriority(holder));
}
```

Using `t->relayed`/`sc->t` here (rather than assuming the SC sits directly
at `t->waitingOn->owner`) is required precisely because the lock's current
owner may have relayed it further along a chain (Section 5) since `t`
first donated it.

`_proc_lockSet()`'s own post-wait revert code is consequently *never*
reachable for the "lock not obtained" branch except after
`_thread_interrupt()` has already run - every path to `-EINTR` from
`_proc_lockWaitWake()` is gated on `interruptible != 0`, which in turn is
only ever set true via `_thread_interrupt()`'s `hal_cpuSetReturnValue()`.
The code asserts this invariant (`current->waitingOn == NULL`) rather than
attempting a second, redundant `_sc_return()`.

## 9. `priority` vs. `priorityBase`

`priority` is the thread's *effective*, possibly-boosted priority.
`priorityBase` is the thread's own, permanent baseline - it must only ever
change via an explicit `proc_threadPriority()` call or at creation.

**Corner case - do not derive `priorityBase` from `scActive`.**
`_sc_updateEffPriority()` sets `t->priority = t->scActive->priority` but
deliberately does *not* also copy `scActive->priorityBase`. `scActive` can
be a donated SC, and copying its base would silently overwrite the
thread's own permanent base with the donor's, leaving the thread unable to
ever revert to its real base once fully un-boosted. Symmetrically,
`_proc_threadSetPriority()`'s clamp compares against `thread->priorityBase`
directly, not `thread->scActive->priorityBase`, for the same reason.

## 10. Thread identity: `runningThread` vs. `current`

`threads_common.current[cpu]` is a `sched_context_t*` - it tracks which SC
is currently earning schedule credit on a given CPU. `_proc_current()`
answers a different question: "which `thread_t` is physically executing on
this CPU right now?"

Before this mechanism was fixed, `_proc_current()` derived its answer as
`threads_common.current[cpu]->t`. This is wrong the moment a thread donates
its own *currently-active* SC while continuing to run (exactly what
happens whenever a thread blocks on a contended lock: `sc == from->scActive
== threads_common.current[cpu]` at that instant) - `_sc_donateAt()`
reassigns that very SC's `.t` field to the recipient, so `_proc_current()`
immediately starts resolving to the *new* SC owner, even though the
donor's code and stack are still what's physically executing, with no real
context switch having happened yet.

The consequence was severe: `_threads_schedule()` (the real scheduler,
invoked via PendSV) does `current = _proc_current(); current->context =
context;`, unconditionally saving the physically-running thread's real
hardware register state into the *wrong* `thread_t`'s `.context` field.
That misfiled context is later restored when the scheduler dispatches the
victim thread by name, causing the donor's code to silently resume running
under a different thread's identity - manifesting as corrupted wait-queue
linkage, threads that never get to run their own code, and any number of
downstream asserts far from the actual cause.

**Fix:** `threads_common.runningThread[]` is a separate, parallel per-CPU
array of `thread_t*`, updated *only* at genuine context-switch points
(`_threads_switchToThread()`, `_threads_switchTo()`, and the IPC fastpath
hand-offs in `proc_forward()`/`proc_respond_ex()`), always from the thread
parameter already in hand at that call site - never derived from an SC's
`.t` field. `_proc_current()` and every "is this thread currently running
on some CPU" check (`_proc_threadSetPriority()`, `_proc_threadDequeue()`,
`proc_threadEnd()`) use this array instead.

**Do not** "simplify" `_proc_runningThread()`/`_proc_current()` back to
deriving identity from `threads_common.current[cpu]->t`. It will
reintroduce this class of bug, and it will do so silently - the corruption
only manifests once contention actually occurs.

## 11. `_proc_lockWaitWake()` must receive `current` as a parameter

For the same reason as Section 10: by the time a waiter's own
`_sc_donate()` call has returned, `_proc_current()` may already resolve to
the lock owner instead of the waiter. `_proc_lockWaitWake()` therefore
takes `current` as an explicit parameter, captured by its caller
(`_proc_lockSet()`) *before* any donation happens, and passes it through
to `_proc_threadEnqueueThread()` directly rather than going through
`_proc_threadEnqueue()` (which would re-derive it via `_proc_current()` and
get it wrong).

## 12. IPC interaction summary

- `msgSend`/`proc_send_ex()`: the caller donates its own `scActive` to the
  receiver via `_sc_donate()`, exactly like a lock donation, and the
  receiver is switched to immediately (`_threads_switchTo()` sets
  `runningThread[cpu]` correctly - no window for the identity bug here,
  since it's a real, synchronous switch, not a deferred reschedule).
- `msgRecv` on an empty queue (`_becomePassive()`): the thread is marked
  passive with `scActive = NULL`, but is *not* away (Section 3) - it's
  simply idle, waiting to be matched.
- `proc_respond_ex()` / forwarding chains: a receiver replying to its
  caller while also forwarding to a new receiver reclaims the caller's SC
  (`_sc_ofDonor` + `_sc_return`) and re-donates it to the new receiver.
  `_sc_return()`'s "non-own SC coming back" assert accepts this case via
  `to->reply != NULL` (the receiver is mid-forward-chain) as an
  alternative to `sc == to->relayed` (the BWI away/chain case) - these are
  genuinely different scenarios that happen to share the same "SC isn't
  `to`'s own" shape.
- A thread that holds a lock and *then* makes a blocking IPC call while
  still holding it is handled correctly by the unified `_sc_awayTarget()`:
  a donation to that thread (for the lock) relays through to whoever it
  called, exactly as it would relay through a second, nested lock.
