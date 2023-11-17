#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

void signalHandler(int signum) {
    // Handle signals if needed
    // For example, handle SIGCHLD to reap zombie processes
}

int main() {
    signal(SIGCHLD, signalHandler);

    int pipefd[2];
    pid_t child_pid;

    // Create a pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Fork a child process
    if ((child_pid = fork()) == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        // Child process (consumer)

        // Close write end of the pipe
        close(pipefd[1]);

        // Read from the pipe
        char receivedString[100];
        while (1) {
            if (read(pipefd[0], receivedString, sizeof(receivedString)) > 0) {
                // Process the received string value
		receivedString[strcspn(receivedString, "\n")] = 0;
                printf("Consumer received: %s\n", receivedString);
            }
        }

        // Close read end of the pipe
        close(pipefd[0]);
    } else {
        // Parent process (abcExec)

        // Close read end of the pipe
        close(pipefd[0]);

        // Redirect standard output to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
	//printf("12345");
        // Execute the abcExec program
        if (execl("./abcExec", "abcExec", NULL) == -1) {
            perror("execl");
            exit(EXIT_FAILURE);
        }

        // Close write end of the pipe
        close(pipefd[1]);
    }

    return 0;
}

