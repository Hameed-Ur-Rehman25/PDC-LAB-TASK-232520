/*
 * Lab 05 - Task 5 (optional): Lock-free stack with compare-and-swap
 * Every thread pushes its own pre-allocated nodes and pops one node after
 * each push, so pushes and pops from all threads interleave. Each pop is
 * recorded in seen[] to detect lost or duplicated nodes.
 *   -DUSE_MUTEX  same test with a mutex-protected stack (for comparison)
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#ifndef NUM_THREADS
#define NUM_THREADS 4
#endif
#ifndef NODES_PER_THREAD
#define NODES_PER_THREAD 100000
#endif

#define TOTAL_NODES (NUM_THREADS * NODES_PER_THREAD)

typedef struct node {
    struct node *next;
    long id;
} node_t;

static node_t *nodes;                    /* all nodes, never freed or reused */
static atomic_uchar seen[TOTAL_NODES];   /* seen[id] = times node id was popped */

#ifdef USE_MUTEX
static node_t *top = NULL;
static pthread_mutex_t stack_lock = PTHREAD_MUTEX_INITIALIZER;
#define VERSION "mutex stack"

static void push(node_t *n)
{
    pthread_mutex_lock(&stack_lock);
    n->next = top;
    top = n;
    pthread_mutex_unlock(&stack_lock);
}

static node_t *pop(void)
{
    pthread_mutex_lock(&stack_lock);
    node_t *n = top;
    if (n != NULL)
        top = n->next;
    pthread_mutex_unlock(&stack_lock);
    return n;
}

static int stack_is_empty(void) { return top == NULL; }
#else
static _Atomic(node_t *) top = NULL;
#define VERSION "lock-free stack"

static void push(node_t *n)
{
    node_t *old_top = atomic_load(&top);
    do {
        n->next = old_top; /* n is still private, so this write is safe */
    } while (!atomic_compare_exchange_weak(&top, &old_top, n));
    /* a failed CAS reloads old_top with the current top and we retry */
}

static node_t *pop(void)
{
    node_t *old_top = atomic_load(&top);
    while (old_top != NULL &&
           !atomic_compare_exchange_weak(&top, &old_top, old_top->next))
        ; /* retry with the reloaded old_top */
    return old_top;
}

static int stack_is_empty(void) { return atomic_load(&top) == NULL; }
#endif

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void *worker(void *arg)
{
    long id = (long)arg;
    node_t *my_nodes = &nodes[id * NODES_PER_THREAD];

    for (long i = 0; i < NODES_PER_THREAD; i++) {
        push(&my_nodes[i]);
        node_t *n;
        while ((n = pop()) == NULL)
            ; /* cannot stay empty: every thread pushes before it pops */
        atomic_fetch_add(&seen[n->id], 1);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[NUM_THREADS];

    nodes = malloc(sizeof(node_t) * TOTAL_NODES);
    for (long i = 0; i < TOTAL_NODES; i++)
        nodes[i].id = i;

    double start = now_ms();
    for (long i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, (void *)i);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    double elapsed = now_ms() - start;

    long lost = 0, duplicated = 0;
    for (long i = 0; i < TOTAL_NODES; i++) {
        unsigned char times = atomic_load(&seen[i]);
        if (times == 0)
            lost++;
        else if (times > 1)
            duplicated++;
    }

    printf("[%s threads=%d] nodes=%d lost=%ld duplicated=%ld stack_empty=%s "
           "time=%.2f ms\n", VERSION, NUM_THREADS, TOTAL_NODES, lost,
           duplicated, stack_is_empty() ? "YES" : "NO", elapsed);
    free(nodes);
    return 0;
}
