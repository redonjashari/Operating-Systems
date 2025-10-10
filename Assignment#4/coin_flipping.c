/*
Assignment #4
Operating Systems
Redon Jashari 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>

#define COIN_COUNT 20
static char coins[COIN_COUNT + 1]; // +1 for '\0'
static int P = 100;       // number of persons / threads
static int N = 10000;     // flips per person

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t coin_locks[COIN_COUNT];

// initialize coins
static void init_coins(void) {
    for (int i = 0; i < COIN_COUNT; ++i) {
        if (i < COIN_COUNT/2) {
            coins[i] = '0';
        } else coins[i] = 'X';
    }
    coins[COIN_COUNT] = '\0';
}

// print coins
static void print_coins(const char *tag) {
    printf("coins: %s (%s)\n", coins, tag);
}

// flip single coin at index i 
static void flip_coin_index(int i) {
    coins[i] = (coins[i] == '0') ? 'X' : '0';
}

/* Strategy 1:
   Each thread acquires the global (table) lock once,
   flips ALL 20 coins N times, then releases the lock.
*/
static void *global_lock_all(void *arg) {
    (void)arg;
    pthread_mutex_lock(&table_lock);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < COIN_COUNT; ++j) {
            flip_coin_index(j);
        }
    }
    pthread_mutex_unlock(&table_lock);
    return NULL;
}

/* Strategy 2:
   Each thread loops N times: acquire global lock, 
   flip all coins once, release.
*/
static void *global_lock_iter(void *arg) {
    (void)arg;
    for (int iter = 0; iter < N; ++iter) {
        pthread_mutex_lock(&table_lock);
        for (int j = 0; j < COIN_COUNT; ++j) {
            flip_coin_index(j);
        }
        pthread_mutex_unlock(&table_lock);
    }
    return NULL;
}

/* Strategy 3:
   Each thread loops N times; for each coin it locks the coin's mutex,
   flips the coin, then unlocks.
*/
static void *sep_coin_lock(void *arg) {
    (void)arg;
    for (int iter = 0; iter < N; ++iter) {
        for (int j = 0; j < COIN_COUNT; ++j) {
            pthread_mutex_lock(&coin_locks[j]);
            flip_coin_index(j);
            pthread_mutex_unlock(&coin_locks[j]);
        }
    }
    return NULL;
}

// run n threads executing proc and join them
static void run_threads(int n, void *(*proc)(void *)) {
    pthread_t *threads = malloc(sizeof(pthread_t) * n); //array
    if (!threads) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; ++i) {
        int rc = pthread_create(&threads[i], NULL, proc, NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n; ++i) {
        //blocks until thread terminates
        pthread_join(threads[i], NULL);
    } 
    free(threads);
}

// defined timeit function
static double timeit(int n, void *(*proc)(void *)) {
    clock_t t1, t2;
    t1 = clock();
    run_threads(n, proc);
    t2 = clock();
    return ((double)(t2 - t1)) / (double)CLOCKS_PER_SEC * 1000.0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-p threads] [-n flips]\n", prog);
    fprintf(stderr, "  -p threads   number of persons/threads (default 100)\n");
    fprintf(stderr, "  -n flips     number of flips per person (default 10000)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "p:n:h")) != -1) {
        switch (opt) {
            case 'p': 
                P = atoi(optarg); 
                if (P <= 0) {
                    usage(argv[0]); 
                    break;
                }
            case 'n': 
                N = atoi(optarg); 
                if (N < 0) {
                    usage(argv[0]); break;
                }
            default: 
                usage(argv[0]); 
                break;
        }
    }

    // initialize coin locks
    for (int i = 0; i < COIN_COUNT; ++i) {
        pthread_mutex_init(&coin_locks[i], NULL);
    }

    //Strategy 1: global lock for entire N
    init_coins();
    print_coins("start - global lock");
    double t1_ms = timeit(P, global_lock_all);
    print_coins("end - global lock");
    printf("%d threads x %d flips: %.3f ms\n\n", P, N, t1_ms);

    // Strategy 2: global lock each iteration 
    init_coins();
    print_coins("start - iteration lock");
    double t2_ms = timeit(P, global_lock_iter);
    print_coins("end - iteration lock");
    printf("%d threads x %d flips: %.3f ms\n\n", P, N, t2_ms);

    // Strategy 3: per-coin locks
    init_coins();
    print_coins("start - coin lock");
    double t3_ms = timeit(P, sep_coin_lock);
    print_coins("end - coin lock");
    printf("%d threads x %d flips: %.3f ms\n\n", P, N, t3_ms);

    // destroy coin locks
    for (int i = 0; i < COIN_COUNT; ++i) {
        pthread_mutex_destroy(&coin_locks[i]);
    }

    return 0;
}
