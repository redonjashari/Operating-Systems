/*
Problem 3.2
Assignment#3
Operating Systems
Redon Jashari 
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int reapall(void)
{
    int status;
    pid_t pid;
    int normal_count = 0;

    for (;;) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            // A child process was reaped
            if (WIFEXITED(status)) {
                normal_count++;
            }
            continue;
        }

        if (pid == 0) {
            // children exist but none have exited right now
            break;
        }

        // if pid == -1 then error or no children processes
        if (errno == EINTR) {
            // interrupted by signal, retry
            continue;
        }
        if (errno == ECHILD) {
            // no child processes
            break;
        }

        // error, return -1
        return -1;
    }
    // returns # of children that terminated normally
    return normal_count;
}
