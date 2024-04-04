#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int fds[2];
  int pid;

  if (pipe(fds) < 0)
  {
    printf("pipe failed\n");
    exit(1);
  };

  pid = fork();
  if (pid == 0)
  {
    char buf[4];
    read(fds[0], buf, 4);
    printf("%d: received %s\n", getpid(), buf);
    write(fds[1], "pong", sizeof(buf));
    exit(0);
  }
  else
  {
    char buf[4];
    write(fds[1], "ping", 4);
    wait(0);
    read(fds[0], buf, sizeof(buf));
    printf("%d: received %s\n", getpid(), buf);
  }
  exit(0);
}