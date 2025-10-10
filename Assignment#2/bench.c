/*
Assignment#2
Problem 2.2
Operating Systems
Redon Jashari
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <limits.h>


static double timespec_to_double(const struct timespec *times) {
    return (double)times->tv_sec + (double)times->tv_nsec / 1e9; //turns nanoseconds into fractional seconds
}

int main(int argc, char *argv[]) {
    int opt;
    long warmups = 0;
    double duration = 5.0;
    int runtime_error = 0;

    /* Parsing the command and options*/
    while ((opt = getopt(argc, argv, "w:d:")) != -1) {
        switch (opt) {
            //Warmup command default 0
            case 'w': {
                char *end;
                errno = 0;
                long val = strtol(optarg, &end, 10);
                if (errno != 0 || *end != '\0' || val < 0) {
                    fprintf(stderr, "Invalid warmups value: %s\n", optarg);
                    return 2;
                }
                warmups = val;
                break;
            }
            //Duration command default 5 sec
            case 'd': {
                char *end;
                errno = 0;
                double v = strtod(optarg, &end);
                if (errno != 0 || *end != '\0' || v < 0.0) {
                    fprintf(stderr, "Invalid duration value: %s\n", optarg);
                    return 2;
                }
                duration = v;
                break;
            }
            default:
                return 2;
        }
    }

    //Command missing an argument to run
    if (optind >= argc) {
        fprintf(stderr, "Missing command to run.\n");
        return 2;
    }

    char **cmd_argv = &argv[optind];

    /* Warmup runs */
    for (long i = 0; i < warmups; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork (warmup)");
            runtime_error = 1;
            break;
        } else if (pid == 0) {
            /*If we are inside the child proccess after fork then replace it with cmd_argv*/
            execvp(cmd_argv[0], cmd_argv); 
            // exec failed
            fprintf(stderr, "execvp failed: %s: %s\n", cmd_argv[0], strerror(errno));
            _exit(127);
        } else {
            // parent: wait for child
            int status;
            while (waitpid(pid, &status, 0) < 0) {
                if (errno == EINTR) continue;
                perror("waitpid (warmup)");
                runtime_error = 1;
                break;
            }
            if (runtime_error) break;
        }
    }

    /* If a runtime error already occurred during warmups then abort */
    if (runtime_error) return 1;

    // Measurement variables
    long runs = 0;
    long fails = 0;
    double total = 0.0;
    double min = 0.0;
    double max = 0.0;

    // Run measured loop until total >= duration
    while (total < duration) {
        struct timespec tstart, tend;
        if (clock_gettime(CLOCK_MONOTONIC, &tstart) != 0) {
            perror("clock_gettime (start)");
            runtime_error = 1;
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            runtime_error = 1;
            break;
        } else if (pid == 0) {
            // child executes the command
            execvp(cmd_argv[0], cmd_argv);
            // execution failed
            fprintf(stderr, "execvp failed: %s: %s\n", cmd_argv[0], strerror(errno));
            _exit(127);
        } else {
            // parent waits
            int status;
            pid_t w;
            for (;;) {
                w = waitpid(pid, &status, 0);
                if (w < 0) {
                    if (errno == EINTR) continue;
                    perror("waitpid");
                    runtime_error = 1;
                }
                break;
            }
            if (runtime_error) break;

            if (clock_gettime(CLOCK_MONOTONIC, &tend) != 0) {
                perror("clock_gettime (end)");
                runtime_error = 1;
                break;
            }

            double elapsed = timespec_to_double(&tend) - timespec_to_double(&tstart);

            /* Update stats */
            if (runs == 0) {
                min = max = elapsed;
            } else {
                if (elapsed < min) min = elapsed;
                if (elapsed > max) max = elapsed;
            }
            total += elapsed;
            runs++;

            /* Determine if this execution failed (non-zero exit or signal) */
            if (WIFEXITED(status)) {
                if (WEXITSTATUS(status) != 0) fails++;
            } else {
                // signaled or otherwise not normal exit
                fails++;
            }
        }
    }

    // Print summary
    if (runs > 0) {
        double avg = total / (double)runs;
        printf("Min: %.6f seconds Warmups: %ld\n", min, warmups);
        printf("Avg: %.6f seconds Runs: %ld\n", avg, runs);
        printf("Max: %.6f seconds Fails: %ld\n", max, fails);
        printf("Total: %.6f seconds\n", total);
    } else {
        //No measured runs (possible if duration == 0)
        printf("Duration was 0 so no measured runs were performed\n");
    }

    // Return non-zero if a runtime error occurred
    return runtime_error ? 1 : 0;
}
