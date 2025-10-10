/*
Problem 3.3
Assignment#3
Operating Systems
Redon Jashari 
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <errno.h>


//defined is_perfect function
static int
is_perfect(uint64_t num)
{
    uint64_t i, sum;

    if (num < 2) {
        return 0;
    }

    for (i = 2, sum = 1; i * i <= num; i++) {
        if (num % i == 0) {
            sum += (i * i == num) ? i : i + num / i;
        }
    }

    return (sum == num);
}

// thread arguments to pass data
struct targs {
    int tid; //threadid
    uint64_t start; //start of range
    uint64_t end; //end of range
    int verbose;
    pthread_mutex_t *out_mtx; //ptr to mutex to protect printf
};

//thread function for thread executions
static void *
thread_func(void *arg)
{
    struct targs *a = (struct targs *)arg;
    if (a->verbose) { // if verbose is enabled then write to stderr
        fprintf(stderr, "perfect: t%d searching [%" PRIu64 ",%" PRIu64 "]\n",
                a->tid, a->start, a->end);
        //PRIu64 is the format to print uint64_t
    }

    for (uint64_t n = a->start; n <= a->end; ++n) {
        if (is_perfect(n)) {
            // protect printf so lines don't interfere
            pthread_mutex_lock(a->out_mtx); //lock mutex
            printf("%" PRIu64 "\n", n); //print
            fflush(stdout); // flush to ensure output is written
            pthread_mutex_unlock(a->out_mtx); //unlock mutex
        }
    }

    //if verbose print
    if (a->verbose) {
        fprintf(stderr, "perfect: t%d finishing\n", a->tid);
    }
    return NULL;
}


int main(int argc, char **argv)
{
    uint64_t start = 1; //default start = 1
    uint64_t end = 10000; //default end = 10000
    int threads = 1; //one thread default
    int verbose = 0;

    // parse through the options
    int opt;
    while ((opt = getopt(argc, argv, "s:e:t:v")) != -1) {
        switch (opt) {
        case 's':
            //using strtoull parse optarg into uint64_t
            start = strtoull(optarg, NULL, 10);
            break;
        case 'e':
            //using strtoull parse optarg into uint64_t
            end = strtoull(optarg, NULL, 10);
            break;
        case 't':
            //parse thread count using strtol
            threads = (int)strtol(optarg, NULL, 10);
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            argv[0];
        }
    }
    
    // ensure start is atleast 1 and end is not smaller than start
    if (start < 1 || end < start) {
        fprintf(stderr, "Invalid range: start must be >=1 and end >= start\n");
        return EXIT_FAILURE;
    }
    // ensure thread > 1
    if (threads < 1) {
        fprintf(stderr, "Invalid thread count: must be >= 1\n");
        return EXIT_FAILURE;
    }

    // create threads and assign contiguous subranges

    // allocate an array of pthread_t to hold tids
    pthread_t *tids = malloc(sizeof(pthread_t) * threads);
    // thread arguments
    struct targs *args = malloc(sizeof(struct targs) * threads);
    pthread_mutex_t out_mtx; // protect output
    pthread_mutex_init(&out_mtx, NULL); // initialize mutex

    uint64_t total = end - start + 1; // inclusive range
    uint64_t base_chunk = total / (uint64_t)threads; // integer division 
    uint64_t remainder = total % (uint64_t)threads; // not evenly divisible integers

    uint64_t cur = start;
    for (int i = 0; i < threads; ++i) { //iterate per thread
        //remainder threads get base_chunk + 1
        uint64_t chunk = base_chunk + (i < (int)remainder ? 1 : 0);
        uint64_t s = cur;
        uint64_t e = cur + chunk - 1;

        //storing
        args[i].tid = i; //thread id
        args[i].start = s; //start of range
        args[i].end = e; //end of range
        args[i].verbose = verbose; //verbose
        args[i].out_mtx = &out_mtx; // share ptr to mutex

        cur += chunk;
    }

    // Launch threads
    for (int i = 0; i < threads; ++i) {
        if (args[i].start > args[i].end) {
            // empty range; create thread that will immediately finish
        }
        if (pthread_create(&tids[i], NULL, thread_func, &args[i]) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    // join threads
    for (int i = 0; i < threads; ++i) {
        pthread_join(tids[i], NULL);
    }

    pthread_mutex_destroy(&out_mtx); // destroy mutex
    free(tids); // free array of tids
    free(args); // free struct args
    return 0;
}
