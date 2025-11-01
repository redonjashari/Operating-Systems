#define _GNU_SOURCE
#include <jansson.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "quiz.h"

static char* xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static char* html_unescape(const char *in) {
    if (!in) return NULL;
    size_t len = strlen(in);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t oi = 0;
    for (size_t i = 0; i < len; ) {
        if (in[i] == '&') {
            if (strncmp(&in[i], "&lt;", 4) == 0) {
                out[oi++] = '<';
                i += 4;
                continue;
            } else if (strncmp(&in[i], "&gt;", 4) == 0) {
                out[oi++] = '>';
                i += 4;
                continue;
            } else if (strncmp(&in[i], "&amp;", 5) == 0) {
                out[oi++] = '&';
                i += 5;
                continue;
            } else if (strncmp(&in[i], "&quot;", 6) == 0) {
                out[oi++] = '"';
                i += 6;
                continue;
            } else if (strncmp(&in[i], "&#039;", 5) == 0) {
                out[oi++] = '\'';
                i += 6;
                continue;
            }
        } 
        out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return out;
}

static void shuffle4(char *choices[4]) {
    for (int i = 3; i > 0; --i) {
        int j = rand() % (i + 1);
        char *temp = choices[i];
        choices[i] = choices[j];
        choices[j] = temp;
    }
}

int parse(quiz_t *quiz, char *msg) {
    if (!quiz || !msg) return -1;

    free_question_fields(quiz);

    json_error_t error;
    json_t *root = json_loads(msg, 0, &error);
    if (!root || !json_is_object(root)) return -1;

    json_t *results = json_object_get(root, "results");
    if (!results || !json_is_array(results) || json_array_size(results) < 1) {
        json_decref(root);
        return -1;
    }

    json_t *qobj = json_array_get(results, 0);
    if (!qobj || !json_is_object(qobj)) {
        json_decref(root);
        return -1;
    }

    const char *qstr = json_string_value(json_object_get(qobj, "question"));
    const char *cstr = json_string_value(json_object_get(qobj, "correct_answer"));
    json_t *inc = json_object_get(qobj, "incorrect_answers");
    if (!qstr || !cstr || !inc || !json_is_array(inc) || json_array_size(inc) < 3) {
        json_decref(root); 
        return -1;
    }
    
    char *choices[4] = {0};
    for (size_t i = 0; i < 3; ++i) {
        const char *s = json_string_value(json_array_get(inc, i));
        if (!s) {
            json_decref(root);
            return -1;
        }
        char *u = html_unescape(s);
        if (!u) {
            json_decref(root);
            return -1;
        }
        choices[i] = u;
    }

    char *u_correct = html_unescape(cstr);
    if (!u_correct) {
        for (size_t i = 0; i < 3; ++i) free(choices[i]);
        json_decref(root);
        return -1;
    }
    choices[3] = u_correct;
    shuffle4(choices);

    int correct_index = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (strcmp(choices[i], u_correct) == 0) {
            correct_index = i;
            break;
        }
    }

    if (correct_index < 0) {
        for (size_t i = 0; i < 4; ++i) free(choices[i]);
        json_decref(root);
        return -1;
    }
    
    char *uq = html_unescape(qstr);
    if (!uq) {
        for (int i = 0; i < 4; ++i) free(choices[i]);
        json_decref(root);
        return -1;
    }

    quiz->question = uq;
    for (int i = 0; i < 4; ++i)
    {
        quiz->choices[i] = choices[i];
    }

    quiz->answer = (char *)malloc(2);
    if (!quiz->answer) {
        free_question_fields(quiz);
        json_decref(root);
        return -1;
    }
    quiz->answer[0] = (char)('a' + correct_index);
    quiz->answer[1] = '\0';
    
    json_decref(root);
    return 0;
}

