/*
Operating Systems
Homework#1
Redon Jashari
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

//buffering modes
enum bufmode { MODE_DEFAULT = 0, MODE_UNBUF, MODE_LINE, MODE_FULL };

//parse positive integer from string; returns -1 on error
static long parse_size(const char *s) {
    char *end;
    errno = 0;
    long val = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || val <= 0) {
        return -1;
    }
    return val;
}

int main(int argc, char *argv[]) {
    int opt;
    int repeat = 1;
    int no_newline = 0;

    enum bufmode mode = MODE_DEFAULT;
    long bufsize = 0;
    char *buf = NULL; //allocated buffer for setvbuf (if needed)

    while ((opt = getopt(argc, argv, "r:nub:l:")) != -1) {
        switch (opt) {
            case 'r':
                repeat = atoi(optarg);
                if (repeat < 0) repeat = 0;
                break;
            case 'n':
                no_newline = 1;
                break;
            case 'u':
                mode = MODE_UNBUF;   // unbuffered
                bufsize = 0;
                break;
            case 'b': {
                long v = parse_size(optarg);
                if (v < 0) {
                    fprintf(stderr, "invalid buffer size for -b: %s\n", optarg);
                    return 1;
                }
                mode = MODE_FULL;    // fully buffered 
                bufsize = v;
                break;
            }
            case 'l': {
                long v = parse_size(optarg);
                if (v < 0) {
                    fprintf(stderr, "invalid buffer size for -l: %s\n", optarg);
                    return 1;
                }
                mode = MODE_LINE;    // line buffered
                bufsize = v;
                break;
            }
            default:
                break;
        }
    }

    //apply buffering choice BEFORE any output to stdout */
    if (mode == MODE_UNBUF) {
        if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
            fprintf(stderr, "setvbuf(_IONBF) failed: %s\n", strerror(errno));
            // not fatal — continue
        }
    } else if (mode == MODE_LINE) {
        if (bufsize <= 0) bufsize = BUFSIZ;
        buf = malloc((size_t)bufsize);
        if (buf == NULL) {
            fprintf(stderr, "malloc failed for line buffer of size %ld\n", bufsize);
            return 1;
        }
        if (setvbuf(stdout, buf, _IOLBF, (size_t)bufsize) != 0) {
            fprintf(stderr, "setvbuf(_IOLBF) failed: %s\n", strerror(errno));
            //we don't free buf here because stdio may use it
        }
    } else if (mode == MODE_FULL) {
        if (bufsize <= 0) bufsize = BUFSIZ;
        buf = malloc((size_t)bufsize);
        if (buf == NULL) {
            fprintf(stderr, "malloc failed for full buffer of size %ld\n", bufsize);
            return 1;
        }
        if (setvbuf(stdout, buf, _IOFBF, (size_t)bufsize) != 0) {
            fprintf(stderr, "setvbuf(_IOFBF) failed: %s\n", strerror(errno));
        }
    }

    // Main printing loop (same behavior as original)
    for (int i = 0; i < repeat; ++i) {
        int first_word = 1;
        for (int j = optind; j < argc; ++j) {
            if (!first_word) {
                putchar(' ');
            }
            first_word = 0;
            char *word = argv[j];
            for (char *p = word; *p != '\0'; ++p) {
                putchar(*p);
            }
        }
        if (!no_newline) {
            putchar('\n');
        }
    }

    return 0;
}
