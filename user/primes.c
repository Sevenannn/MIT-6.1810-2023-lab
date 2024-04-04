#include "kernel/types.h"
#include "user/user.h"

void
helper(int left_fds[]) {
  int pid;
  int p;

  pid = fork();
  if (pid == 0) {
    if (read(left_fds[0], &p, sizeof(p)) == 0) {
      return;
    }
    printf("prime %d\n", p);

    int n;
    int right_fds[2];
    pipe(right_fds);
    int read_result;

    while ((read_result = read(left_fds[0], &n, sizeof(n))) > 0) {
      if (n % p != 0) {
        write(right_fds[1], &n, sizeof(n));
      }
    }

    close(left_fds[0]);
    close(right_fds[1]);
    helper(right_fds);
  }
  else {
    wait(0);
    close(left_fds[1]);
  }
}

int
main(int argc, char *argv[])
{
  int low = 2;
  int high = 35;
  int fds[2];

  pipe(fds);
  for (int i = low; i <= high; i++) {
    write(fds[1], &i, sizeof(i));
  }

  close(fds[1]);
  helper(fds);
  exit(0);
}