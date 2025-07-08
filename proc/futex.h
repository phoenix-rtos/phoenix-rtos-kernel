#ifndef _SYS_FUTEX_H_
#define _SYS_FUTEX_H_

#include "lib/lib.h"
#include "hal/hal.h"
#include "threads.h"

/*
 * For now we only have private futexes. Private means that a futex is within a single process'
 * address space. Shared would mean that futexes can be used betweem different address spaces.
 */

/* wait queue node */
typedef struct _futex_wq_node_t {
    struct _futex_wq_node_t *prev, *next;
    struct _thread_t *thread;
} futex_wq_node_t;

typedef struct _futex_t {
    futex_wq_node_t *wait_queue;
    spinlock_t spinlock;
} futex_t;

typedef struct _ft_node_t {
    addr_t key;
    futex_t *value;
    int taken;
} ft_node_t;

#define FUTEX_TABLE_SIZE 10

typedef struct _futex_table_t {
    ft_node_t table[FUTEX_TABLE_SIZE];
} futex_table_t;

int proc_futexWakeup(struct _process_t *process, int *address, int value, int n_threads);
int proc_futexWait(struct _process_t *process, const int *const address, const int value, const time_t timeout);
futex_table_t *proc_newFutexTable(void);
void proc_freeFutexTable(futex_table_t *ft);

#endif
