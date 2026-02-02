#include "common.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *a) {
    fprintf(stderr, "Usage: %s <pid>\n", a);
    exit(2);
}

static int isnum(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static int read_all(const char *path, char *buf, size_t cap, size_t *n_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (n_out) *n_out = n;
    return 0;
}

int main(int c, char **v) {
    if (c != 2 || !isnum(v[1])) usage(v[0]);

    errno = 0;
    long pid_l = strtol(v[1], NULL, 10);
    if (errno != 0 || pid_l <= 0) usage(v[0]);
    int pid = (int)pid_l;

    char path[256];

    char statbuf[8192];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (read_all(path, statbuf, sizeof(statbuf), NULL) != 0) {
        if (errno == ENOENT) fprintf(stderr, "procinfo: pid not found\n");
        else if (errno == EACCES) fprintf(stderr, "procinfo: permission denied\n");
        else fprintf(stderr, "procinfo: cannot read stat: %s\n", strerror(errno));
        return 1;
    }

    char *rparen = strrchr(statbuf, ')');
    if (!rparen) {
        fprintf(stderr, "procinfo: failed to parse stat\n");
        return 1;
    }

    char *p = rparen + 1;
    while (*p == ' ') p++;

    char state = '?';
    int ppid = -1;
    unsigned long long utime = 0, stime = 0;

    char *save = NULL;
    char *tok = strtok_r(p, " ", &save);
    if (tok && tok[0]) state = tok[0];

    tok = strtok_r(NULL, " ", &save);
    if (tok) ppid = atoi(tok);

    int idx = 1;
    while ((tok = strtok_r(NULL, " ", &save)) != NULL) {
        idx++;
        if (idx == 11) utime = strtoull(tok, NULL, 10);
        else if (idx == 12) { stime = strtoull(tok, NULL, 10); break; }
    }

    unsigned long long ticks = utime + stime;
    long hz = sysconf(_SC_CLK_TCK);
    double seconds = (hz > 0) ? ((double)ticks / (double)hz) : 0.0;

    char cmdbuf[8192];
    size_t cmdn = 0;
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    if (read_all(path, cmdbuf, sizeof(cmdbuf), &cmdn) != 0 || cmdn == 0 || cmdbuf[0] == '\0') {
        strcpy(cmdbuf, "[unknown]");
    }

    long vmrss_kb = -1;
    char statusbuf[8192];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    if (read_all(path, statusbuf, sizeof(statusbuf), NULL) == 0) {
        char *line = statusbuf;
        while (line && *line) {
            char *next = strchr(line, '\n');
            if (next) *next = '\0';
            if (strncmp(line, "VmRSS:", 6) == 0) {
                char *q = line + 6;
                while (*q == ' ' || *q == '\t') q++;
                vmrss_kb = strtol(q, NULL, 10);
                break;
            }
            line = next ? (next + 1) : NULL;
        }
    }

    printf("PID:%d\n", pid);
    printf("State:%c\n", state);
    printf("PPID:%d\n", ppid);
    printf("Cmd:%s\n", cmdbuf);
    printf("CPU:%llu %.3f\n", ticks, seconds);
    if (vmrss_kb >= 0) printf("VmRSS:%ld\n", vmrss_kb);
    else printf("VmRSS:0\n");

    return 0;
}