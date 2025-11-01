/*
 * quiz/quiz.h --
 */
#ifndef QUIZ_H
#define QUIZ_H

#include <signal.h>

typedef struct {
    unsigned n;         /* current question number (starting at 1) */
    unsigned score;     /* current total score */
    unsigned max;       /* possible max score (sum of per-question maxes) */
    char *question;     /* next question (dynamically allocated) */
    char *answer;       /* expected answer letter "a"/"b"/"c"/"d" (malloc'ed) */
    char *choices[4];   /* choices in order a..d (each malloc'ed) */
} quiz_t;

/*
 * Fetch the content of the given url by running 'curl' as a child
 * process and reading the response from a pipe. The response is
 * returned as a malloc'ed string, or NULL if there was an error.
 *
 * Implemented in fetch.c
 */
extern char* fetch(char *url);

/*
 * Parse a JSON encoded msg and fill the next question into the
 * quiz_t. Uses the jansson JSON library. Returns 0 on success or -1
 * if there was a parsing/validation error.
 *
 * Implemented in parse.c
 */
extern int parse(quiz_t *quiz, char *msg);

/*
 * Play one round of the quiz game by first fetching and parsing a
 * quiz question and then interacting with the user. Returns 0 on
 * success or -1 on error. If the user ends stdin during this round,
 * play() prints the current score (as in the example) and returns 0.
 *
 * Implemented in play.c
 */
extern int play(quiz_t *quiz);

/* global flag set by SIGINT handler in quiz.c */
extern volatile sig_atomic_t g_stop;

#endif /* QUIZ_H */
