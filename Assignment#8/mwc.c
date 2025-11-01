/*
Assignment#8
Operating Systems
Redon Jashari
*/

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    unsigned long long lines;
    unsigned long long words;
    unsigned long long bytes;
} Counts;

typedef struct {
    const char *path;  // filename (for printing), NULL means stdin
    int index;         // output order index
    int errnum;        // 0 on success, errno on failure
    Counts counts;     // results
} Job;

static inline void count_buffer(const unsigned char *buf, size_t n, Counts *c, bool *in_word) {
    //update counts for this buffer, carrying in/out word state across buffers.
    for (size_t i = 0; i < n; ++i) {
        unsigned char ch = buf[i];
        if (ch == '\n') c->lines++;
        if (isspace((int)ch)) {
            *in_word = false;
        } else {
            if (!*in_word) {
                c->words++;
                *in_word = true;
            }
        }
    }
    c->bytes += (unsigned long long)n;
}

static int count_fd_stream(int fd, Counts *out) {
    const size_t BUFSZ = 1 << 16;
    unsigned char *buf = (unsigned char *)malloc(BUFSZ);
    if (!buf) return ENOMEM;

    Counts c = {0,0,0};
    bool in_word = false;

    for (;;) {
        ssize_t r = read(fd, buf, BUFSZ);
        if (r > 0) {
            count_buffer(buf, (size_t)r, &c, &in_word);
            continue;
        }
        if (r == 0) break; // EOF
        if (errno == EINTR) continue;
        int e = errno;
        free(buf);
        return e;
    }

    free(buf);
    *out = c;
    return 0;
}

static int count_regular_file_mmap(int fd, off_t fsize, Counts *out) {
    if (fsize == 0) { *out = (Counts){0,0,0}; return 0; }

    // map the entire file read-only, private
    void *map = mmap(NULL, (size_t)fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        return errno;
    }

    Counts c = {0,0,0};
    bool in_word = false;
    const unsigned char *p = (const unsigned char *)map;
    count_buffer(p, (size_t)fsize, &c, &in_word);

    int rc = munmap(map, (size_t)fsize);
    if (rc != 0) {
        // keep the counts but surface unmap error as failure
        return errno;
    }
    *out = c;
    return 0;
}

static void *worker(void *arg) {
    Job *job = (Job *)arg;
    job->errnum = 0;
    job->counts = (Counts){0,0,0};

    int fd = -1;
    struct stat st;

    if (!job->path) {
        // stdin path
        fd = STDIN_FILENO;
        // always stream for stdin
        job->errnum = count_fd_stream(fd, &job->counts);
        return NULL;
    }

    fd = open(job->path, O_RDONLY);
    if (fd < 0) {
        job->errnum = errno;
        return NULL;
    }

    if (fstat(fd, &st) != 0) {
        job->errnum = errno;
        close(fd);
        return NULL;
    }

    if (S_ISREG(st.st_mode)) {
        // prefer mmap for regular files
        int rc = count_regular_file_mmap(fd, st.st_size, &job->counts);
        if (rc != 0) {
            // fall back to streaming on mmap failure
            rc = count_fd_stream(fd, &job->counts);
        }
        job->errnum = rc;
    } else {
        // non-regular: stream
        job->errnum = count_fd_stream(fd, &job->counts);
    }

    close(fd);
    return NULL;
}

static void print_one(const Counts *c, const char *name_or_null) {
    if (name_or_null) {
        // lines words bytes filename
        printf("%llu %llu %llu %s\n",
               (unsigned long long)c->lines,
               (unsigned long long)c->words,
               (unsigned long long)c->bytes,
               name_or_null);
    } else {
        // stdin case: no filename
        printf("%llu %llu %llu\n",
               (unsigned long long)c->lines,
               (unsigned long long)c->words,
               (unsigned long long)c->bytes);
    }
}

int main(int argc, char **argv) {
    //no arguments: read stdin (single-threaded)
    if (argc == 1) {
        Job j = {.path = NULL, .index = 0, .errnum = 0};
        worker(&j);
        if (j.errnum != 0) {
            fprintf(stderr, "mwc: stdin: %s\n", strerror(j.errnum));
            return 1;
        }
        print_one(&j.counts, NULL);
        return 0;
    }

    int n = argc - 1;
    Job *jobs = (Job *)calloc((size_t)n, sizeof(Job));
    pthread_t *tids = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    if (!jobs || !tids) {
        fprintf(stderr, "mwc: allocation failure\n");
        free(jobs);
        free(tids);
        return 1;
    }

    for (int i = 0; i < n; ++i) {
        jobs[i].path = argv[i + 1];
        jobs[i].index = i;
        int rc = pthread_create(&tids[i], NULL, worker, &jobs[i]);
        if (rc != 0) {
            jobs[i].errnum = rc;
            //no thread created; leave counts zero.
        }
    }

    //join and accumulate totals in input order
    Counts total = {0,0,0};
    int ok_files = 0;

    for (int i = 0; i < n; ++i) {
        // Only join if the thread was created successfully
        if (jobs[i].errnum == 0) (void)pthread_join(tids[i], NULL);
        else {
            if (jobs[i].errnum == EAGAIN) {
                worker(&jobs[i]);
            }
        }

        if (jobs[i].errnum != 0) {
            fprintf(stderr, "mwc: %s: %s\n", jobs[i].path, strerror(jobs[i].errnum));
            continue;
        }

        print_one(&jobs[i].counts, jobs[i].path);
        total.lines += jobs[i].counts.lines;
        total.words += jobs[i].counts.words;
        total.bytes += jobs[i].counts.bytes;
        ok_files++;
    }

    if (ok_files >= 2) {
        printf("%llu %llu %llu total\n",
               (unsigned long long)total.lines,
               (unsigned long long)total.words,
               (unsigned long long)total.bytes);
    }

    free(jobs);
    free(tids);
    return 0;
}
