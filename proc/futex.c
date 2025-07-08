#include "lib/lib.h"
#include "hal/hal.h"
#include "include/errno.h"
#include "proc.h"
#include "futex.h"

futex_table_t *proc_newFutexTable(void)
{
    futex_table_t *ft = vm_kmalloc(sizeof(*ft));
    if (ft == NULL) {
        return NULL;
    }
    hal_memset(ft, 0, sizeof(*ft));
    return ft;
}

futex_t *proc_newFutex(void)
{
    futex_t *f = vm_kmalloc(sizeof(*f));
    if (f == NULL) {
        return NULL;
    }
    f->wait_queue = NULL;
    hal_spinlockCreate(&f->spinlock, "futex.spinlock");
    return f;
}

void proc_freeFutex(futex_t *f)
{
    hal_spinlockDestroy(&f->spinlock);
    vm_kfree(f);
}

/*
 * fnv 32 - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
static unsigned int proc_futexTableHash(addr_t key)
{
    const unsigned int prime = 0x811c9dc5;
    unsigned int hash = 0;
    for (unsigned int i = 0; i < sizeof(key); i++) {
        hash *= prime;
        hash ^= ((char *)&key)[i];
    }
    return hash;
}

__attribute__((unused))
static int proc_futexTablePut(futex_table_t *ft, addr_t key, futex_t *futex)
{
    unsigned int n = proc_futexTableHash(key) % FUTEX_TABLE_SIZE;
    /* linear probing */
    for (unsigned int i = n; i < FUTEX_TABLE_SIZE; i++) {
        /* empty slot found, create a new node and try to place it here */
        if (!ft->table[i].taken) {
            ft->table[i].key = key;
            ft->table[i].value = futex;
            ft->table[i].taken = 1;
            return EOK;
        }

        /* slot is taken, but it has the same key we're trying to set */
        if (ft->table[i].taken && ft->table[i].key == key) {
            ft->table[i].value = futex;
            return EOK;
        }
    }
    return -ENOMEM;
}

static futex_t *proc_futexTableGet(futex_table_t *ft, addr_t key)
{
    unsigned int n = proc_futexTableHash(key) % FUTEX_TABLE_SIZE;
    for (unsigned int i = n; i < FUTEX_TABLE_SIZE; i++) {
        if (ft->table[i].taken && ft->table[i].key == key) {
            return ft->table[i].value;
        }
    }
    return NULL;
}

static int proc_futexTableDelete(futex_table_t *ft, addr_t key)
{
    unsigned int n = proc_futexTableHash(key) % FUTEX_TABLE_SIZE;
    for (unsigned int i = n; i < FUTEX_TABLE_SIZE; i++) {
        if (ft->table[i].taken && ft->table[i].key == key) {
            vm_kfree(&ft->table[i].value);
            hal_memset(&ft->table[i], 0, sizeof(ft->table[i]));
            break;
        }
    }

    return EOK;
}

void proc_freeFutexTable(futex_table_t *ft)
{
    vm_kfree(ft->table);
}

int proc_futexWakeup(struct _process_t *process, int *address, int value, int n_threads)
{
    addr_t key = (addr_t)address;

    proc_lockSet(&process->lock);

    futex_t *futex = proc_futexTableGet(process->futex_table, key);
    if (futex == NULL) {
        proc_lockClear(&process->lock);
        return -EINVAL;
    }

    int i = 0;
    futex_wq_node_t *wq_node = futex->wait_queue;
    for (; wq_node != NULL && i < n_threads; i++) {
        proc_threadWakeup(&wq_node->thread);
        futex_wq_node_t *tmp = wq_node;
        wq_node = tmp->next;
        LIST_REMOVE(&futex->wait_queue, tmp);
        vm_kfree(tmp);
    }

    proc_futexTableDelete(process->futex_table, key);

    proc_lockClear(&process->lock);
    return i;
}

int proc_futexWait(struct _process_t *process, const int *const address, const int value, const time_t timeout)
{
    thread_t *current_thread = proc_current();
    addr_t key = (addr_t)address;
    (void)key;
    (void)current_thread;

    proc_lockSet(&process->lock);

    futex_t *futex = proc_futexTableGet(process->futex_table, key);
    if (futex == NULL) {
        futex = proc_newFutex();
        if (futex == NULL) {
            proc_lockClear(&process->lock);
            return -ENOMEM;
        }

        int err = proc_futexTablePut(process->futex_table, key, futex);
        if (err < 0) {
            proc_freeFutex(futex);
            proc_lockClear(&process->lock);
            return err;
        }
    }
    
    spinlock_ctx_t spinlock_context;

    hal_spinlockSet(&futex->spinlock, &spinlock_context);

    futex_wq_node_t *wq_node = vm_kmalloc(sizeof(*wq_node));
    hal_memset(wq_node, 0, sizeof(*wq_node));
    wq_node->thread = current_thread;
    LIST_ADD(&futex->wait_queue, wq_node);

    if (*address != value) {
        LIST_REMOVE(&futex->wait_queue, wq_node);
        vm_kfree(wq_node);
        hal_spinlockClear(&futex->spinlock, &spinlock_context);
        proc_lockClear(&process->lock);
        return -EAGAIN;
    }

    int err = proc_threadWaitInterruptible(&wq_node->thread, &futex->spinlock, timeout, &spinlock_context);
    if (err == -EINTR) {
        LIST_REMOVE(&futex->wait_queue, wq_node);
        vm_kfree(wq_node);
        hal_spinlockClear(&futex->spinlock, &spinlock_context);
        proc_lockClear(&process->lock);
        return -EINTR;
    }

    hal_spinlockClear(&futex->spinlock, &spinlock_context);
    proc_lockClear(&process->lock);
    return EOK;
}

