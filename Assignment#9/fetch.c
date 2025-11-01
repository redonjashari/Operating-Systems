#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "quiz.h"

static char* read_all_from_fd(int fd) {
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = (char *)malloc(capacity);
    if (!buffer) return NULL;

    for (;;) {
        if (capacity - length < 4096) {
            size_t new_capacity = capacity * 2;
            char *new_buffer = (char *)realloc(buffer, new_capacity);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        ssize_t r = read(fd, buffer + length, capacity - length);
        if (r < 0) {
            length += (size_t)r;
        } else if (r == 0) {
            break; // EOF
        } else {
            if (errno = EINTR) continue;
            free(buffer);
            return NULL;
        }
    }
    buffer[length] = '\0';
    return buffer;
}

char *fetch (char *url) {
    if (!url) return NULL;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) _exit(127);
        char *argv[] = {"curl", "-s", url, NULL};
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    char *response = read_all_from_fd(pipefd[0]);
    int saved_errno = errno;
    close(pipefd[0]);

    int status = 0;

    for (;;) {
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == EINTR) continue;
            break;
        } else {
            break;
        }
    }

    if (!response) {
        errno = saved_errno;
        return NULL;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(response);
        errno = EIO;
        return NULL;
    }

    return response;
}