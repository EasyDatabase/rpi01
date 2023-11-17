// consumer.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <mariadb/mysql.h>

// MariaDB Connection parameters^M
#define DB_HOST "localhost"
#define DB_USER "scott"
#define DB_PASSWORD "tiger"
#define DB_NAME "mydb"

void signalHandler(int signum) {
    // Handle signals if needed
    // For example, handle SIGCHLD to reap zombie processes
}

void saveToMariaDB(const char *value) {
    MYSQL *conn = mysql_init(NULL);
    char senval[2][10];

    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(EXIT_FAILURE);
    }


    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }

    char *token = strtok((char *)value, ":");
   
    // Keep printing tokens while one of the
    // delimiters present in str[].
    int i=0;
    while (token != NULL)
    {
	strcpy( senval[i], token);
        printf("%s\n", token);
        token = strtok(NULL, ":");
	i++;
    }

    char query[256];
    for(int i=0; i < 2; i++) {
	    sprintf(query, "INSERT INTO SensorData (sensor_id, reading, timestamp) VALUES (%d, '%s', sysdate())", (i+1), senval[i]);

	    if (mysql_query(conn, query) != 0) {
		fprintf(stderr, "MariaDB query execution failed: %s\n", mysql_error(conn));
	    }
    }

    mysql_close(conn);
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

                // Save to MariaDB
                saveToMariaDB(receivedString);
            }
        }

        // Close read end of the pipe
        close(pipefd[0]);
    } else {
        // Parent process (temperature)

        // Close read end of the pipe
        close(pipefd[0]);

        // Redirect standard output to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);

        // Execute the temperature program
        if (execl("./temperature", "temperature", NULL) == -1) {
            perror("execl");
            exit(EXIT_FAILURE);
        }

        // Close write end of the pipe
        close(pipefd[1]);
    }

    return 0;
}

