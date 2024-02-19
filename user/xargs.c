//
// Created by juven on 2024/2/19.
//

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

char *arg[MAXARG];
char buf[32];
int
readline()
{
    char* c = buf;
    while (read(0, c, 1)) {
        if (*c == '\n') {
            *c = 0;
            return 1;
        }
        c ++;
    }
    *c = 0;
    return 0;
}

int
main(int argc, char *argv[])
{
    arg[0] = "grep";
    int c = 1;
    for (int i = 2; i < argc; i ++) {
        arg[c ++] = argv[i];
    }
    while (readline()) {
        int sz = strlen(buf);
        char* s = malloc(sz + 1);
        memmove(s, buf, sz);
        s[sz] = 0;
        arg[c] = s; arg[c + 1] = 0;
        if (fork() == 0) {
            wait(0);
        } else {
            exec(argv[1], arg);
        }
    }
    exit(0);
}