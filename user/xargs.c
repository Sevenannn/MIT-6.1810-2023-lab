#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

void
runline(char *args[]) {
    int pid;

    pid = fork();
    if (pid == 0) {
        exec(args[0], args);
    } else {
        wait((int *) 0);
    }
}

int
main(int argc, char *argv[])
{
    if (argc <= 1) {
        fprintf(2, "usage: xargs <command> [argv...]\n");
        exit(1);
    }

    char *args[MAXARG];
    char buf[512], *buf_p;
    buf_p = buf;

    for (int i = 1; i < argc; i++) {
        args[i - 1] = argv[i];
    }

    while (read(0, buf_p, 1) > 0) {
        if (*buf_p == '\n') {
			*buf_p = 0;

            args[argc - 1] = buf;
            runline(args);

            memset(buf, 0, sizeof(buf));
            buf_p = buf;
		} else {
            buf_p++;
        }
    }

    exit(0);
}