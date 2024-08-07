#include <unistd.h>
#include <stdio.h>
#include <string.h>

char *message = "This is a message from parent!!!";
char *message2 = "This is a message from child!!!";

int main() {
    pid_t childpid;
    char buf[1024];
    char buf2[1024];
    int parent_to_child[2], child_to_parent[2];

    // Create two pipes: one for parent-to-child, one for child-to-parent
    pipe(parent_to_child);
    pipe(child_to_parent);

    if (fork() != 0) { // Parent process
        // Close unused ends of pipes
        close(parent_to_child[0]); // Close read end of parent-to-child pipe
        close(child_to_parent[1]); // Close write end of child-to-parent pipe

        // Send message to child
        write(parent_to_child[1], message, strlen(message) + 1);

        // Read message from child
        read(child_to_parent[0], buf2, sizeof(buf2));

        printf("I got the following message from my child: %s\n", buf2);

        // Close used ends of pipes
        close(parent_to_child[1]);
        close(child_to_parent[0]);
    } else { // Child process
        // Close unused ends of pipes
        close(parent_to_child[1]); // Close write end of parent-to-child pipe
        close(child_to_parent[0]); // Close read end of child-to-parent pipe

        childpid = getpid();
        printf("CHILD: I am the child process!\n");
        printf("CHILD: Here's my PID: %d\n", childpid);
        printf("CHILD: My parent's PID is: %d\n", getppid());

        // Read message from parent
        read(parent_to_child[0], buf, sizeof(buf));

        printf("I got the following message from my parent: %s\n", buf);

        // Send message back to parent
        write(child_to_parent[1], message2, strlen(message2) + 1);

        // Close used ends of pipes
        close(parent_to_child[0]);
        close(child_to_parent[1]);
    }

    return 0;
}
