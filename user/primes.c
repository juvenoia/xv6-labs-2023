#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int pf[2], ps[2];

int
main(int argc, char *argv[])
{
  pipe(ps);
  char s[1];
  int i;
  for (i = 2; i < 36; i ++) {
    s[0] = i;
    write(ps[1], s, 1);
  }
  close(ps[1]);
  if (fork() == 0) {
start:
    pf[0] = ps[0], pf[1] = ps[1];
    pipe(ps);
    int i = 0, p = 0;
    char t[1];
    while (read(pf[0], t, 1) != 0) {
      if (i == 0) {
        printf("prime %d\n", (int)t[0]);
        p = t[0];
      } else {
        if (t[0] % p != 0)
          write(ps[1], t, 1);
      }
      i ++;
    }
    close(pf[0]);
    close(ps[1]);
    if (fork() == 0)
      goto start;
    else
      wait(0);
    exit(0);
  } else {
    wait(0);
    exit(0);
  }
}
