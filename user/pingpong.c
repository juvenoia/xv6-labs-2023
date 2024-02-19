#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int p[2];
char s[1];

int
main(int argc, char *argv[])
{
  pipe(p);
  write(p[1], "f", 1);
  if (fork() != 0) {
    wait(0);
    read(p[0], s, 1);
    int t = getpid();
    fprintf(0, "%d: received ping\n", t);
    exit(0);
  } else {
    read(p[0], s, 1);
    int t = getpid();
    fprintf(0, "%d: received pong\n", t);
    write(p[1], "s", 1);
    exit(0);
  }
}
