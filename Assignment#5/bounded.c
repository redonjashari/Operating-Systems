/*
Operating Systems
Assignment #4
Redon Jashari
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#undef NDEBUG
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#define BUFFER_SIZE 12

typedef struct buffer {
    unsigned int    data[BUFFER_SIZE];
    int             in;
    int             out;
    pthread_mutex_t mutex;
    sem_t           empty; // #counts free slots 
    sem_t           full;  // #filled slots 
} buffer_t;

// checkerboard: atomic per-slot verification (0 = empty/unset)
static atomic_uint checker[BUFFER_SIZE];

// sequence generator so every produced item gets a unique increasing id
static atomic_uint global_produced_seq = ATOMIC_VAR_INIT(0);
// expected sequence consumed
static atomic_uint global_consumed_seq = ATOMIC_VAR_INIT(0);

static buffer_t shared_buffer = {
    .in = 0,
    .out = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

//initialize semaphores and checker 
static void
buffer_init(buffer_t *b)
{
    sem_init(&b->empty, 0, BUFFER_SIZE);
    sem_init(&b->full,  0, 0);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        atomic_store_explicit(&checker[i], 0u, memory_order_relaxed);
    }
}

// produce returns a strictly increasing positive integer
static unsigned int
produce(void)
{
    //atomic_fetch_add returns the previous value, +1 to make it start from 1
    unsigned int prev = atomic_fetch_add_explicit(&global_produced_seq, 1u, memory_order_relaxed);
    return prev + 1u;
}

// consume validation checks that items are consumed in order. 
static void
consume(unsigned int num)
{
    unsigned int prev = atomic_fetch_add_explicit(&global_consumed_seq, 1u, memory_order_relaxed);
    unsigned int expected = prev + 1u;
    assert(num == expected);
}

//producer: wait on empty, then write item and publish
static void*
producer(void *data)
{
    buffer_t *buffer = (buffer_t *) data;

    while (1) {
        unsigned int item = produce();

        /* wait until a free slot exists (blocking) */
        sem_wait(&buffer->empty);

        /* we need mutual exclusion for updating index and placing the item */
        pthread_mutex_lock(&buffer->mutex);

        int idx = buffer->in;
        buffer->data[idx] = item;

        // publish the value for checkerboard
        atomic_store_explicit(&checker[idx], item, memory_order_release);

        buffer->in = (buffer->in + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&buffer->mutex);

        //full slot is available 
        sem_post(&buffer->full);
    }
    return NULL;
}

// wait on full, then read and validate via checker
static void*
consumer(void *data)
{
    buffer_t *buffer = (buffer_t *) data;

    while (1) {
        // block until there is at least one filled slot
        sem_wait(&buffer->full);

        pthread_mutex_lock(&buffer->mutex);

        int idx = buffer->out;

        // first load the checker with acquire semantics to ensure we see the writer's stores
        unsigned int seen = atomic_load_explicit(&checker[idx], memory_order_acquire);

        // if seen is zero, that would mean reader raced the writer
        unsigned int item = buffer->data[idx];

        // clear the checker slot for future use
        atomic_store_explicit(&checker[idx], 0u, memory_order_relaxed);

        buffer->out = (buffer->out + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&buffer->mutex);

        //free slot 
        sem_post(&buffer->empty);

        consume(item);

    }
    return NULL;
}

static int
run(int nc, int np)
{
    int err, n = nc + np;
    pthread_t thread[n];

    buffer_init(&shared_buffer);

    for (int i = 0; i < n; i++) {
        err = pthread_create(&thread[i], NULL,
                             i < nc ? consumer : producer, &shared_buffer);
        if (err) {
            fprintf(stderr, "bounded: %s(): unable to create thread %d: %s\n",
                    __func__, i, strerror(err));
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < n; i++) {
        if (thread[i]) {
            err = pthread_join(thread[i], NULL);
            if (err) {
                fprintf(stderr, "bounded: %s(): unable to join thread %d: %s\n",
                        __func__, i, strerror(err));
            }
        }
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
    int c, nc = 1, np = 1;

    while ((c = getopt(argc, argv, "c:p:h")) >= 0) {
        switch (c) {
        case 'c':
            if ((nc = atoi(optarg)) <= 0) {
                fprintf(stderr, "number of consumers must be > 0\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'p':
            if ((np = atoi(optarg)) <= 0) {
                fprintf(stderr, "number of producers must be > 0\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            printf("Usage: %s [-c consumers] [-p producers] [-h]\n", argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    return run(nc, np);
}
