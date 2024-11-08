#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

int main() {
    pid_t childpid;
    char buf[5];
    char return_buf[5];
    int fd1[2];
    int fd2[2];

    pipe(fd1);
    pipe(fd2);

    childpid = fork();

    if (childpid > 0) {
        close(fd1[0]);
        close(fd2[1]);

        printf("Waiting for PIN... ");
        scanf("%4s", buf);

        write(fd1[1], buf, strlen(buf));

        read(fd2[0], return_buf, 4);

        printf("Generated PIN: %s\n", return_buf);

        close(fd1[1]);
        close(fd2[0]);
    } else {
        close(fd1[1]);
        close(fd2[0]);

        read(fd1[0], buf, 4);
        buf[4] = '\0';

        srand(getppid() ^ getpid());

        int pin = rand() % 10000;

        snprintf(return_buf, 5, "%04d", pin);

        write(fd2[1], return_buf, 4);

        close(fd1[0]);
        close(fd2[1]);
    }

    return 0;
}