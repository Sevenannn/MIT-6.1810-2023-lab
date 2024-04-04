#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int time;

    if (argc < 2)
    {
        fprintf(2, "Usage: sleep time\n");
        exit(1);
    }
    time = (int)*argv[1];
    sleep(time);
    exit(0);
}