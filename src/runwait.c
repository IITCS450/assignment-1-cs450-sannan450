#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *a){fprintf(stderr,"Usage: %s <cmd> [args]\n",a); exit(2);}

static double d(struct timespec a, struct timespec b){
    long s = b.tv_sec - a.tv_sec;
    long ns = b.tv_nsec - a.tv_nsec;
    if (ns < 0) { s--; ns += 1000000000L; }
    return (double)s + (double)ns / 1e9;
}

int main(int c,char**v){
    if(c < 2) usage(v[0]);

    struct timespec t0, t1;
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) { perror("clock_gettime"); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        execvp(v[1], &v[1]);
        fprintf(stderr, "execvp failed: %s\n", strerror(errno));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }

    if (clock_gettime(CLOCK_MONOTONIC, &t1) != 0) { perror("clock_gettime"); return 1; }

    double elapsed = d(t0, t1);

    if (WIFEXITED(status)) {
        printf("pid=%d elapsed=%.3f exit=%d\n", pid, elapsed, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("pid=%d elapsed=%.3f signal=%d\n", pid, elapsed, WTERMSIG(status));
    } else {
        printf("pid=%d elapsed=%.3f status=%d\n", pid, elapsed, status);
    }

    return 0;
}