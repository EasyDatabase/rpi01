// consumer.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <mariadb/mysql.h>
//#include <mqtt/MQTTClient.h>
#include <MQTTClient.h>

// MariaDB Connection parameters^M
#define DB_HOST "localhost"
#define DB_USER "scott"
#define DB_PASSWORD "tiger"
#define DB_NAME "mydb"

#define MAX_TEXT 512
//#define MQTT_BROKER_ADDRESS "tcp://mqtt.eclipse.org:1883"
//#define MQTT_CLIENT_ID "temperature_sensor_client"
//#define MQTT_TOPIC "output_data"

//#define MQTT_BROKER_ADDRESS "tcp://broker.hivemq.com:1883"
//#define MQTT_CLIENT_ID "temperature_sensor_client"
//#define MQTT_TOPIC "test/temphumi"

#define MQTT_BROKER_ADDRESS "tcp://localhost:1883"
#define MQTT_CLIENT_ID "temperature_sensor_client"
#define MQTT_TOPIC "test/temphumi"
#define MQTT_TOPIC1 "test/temp"
#define MQTT_TOPIC2 "test/humi"

char topics[2][10] = {"test/temp", 
                     "test/humi"};

struct message {
    long msg_type;
    char msg_text[MAX_TEXT];
};


void signalHandler(int signum) {
    // Handle signals if needed
    // For example, handle SIGCHLD to reap zombie processes
}

void saveToMariaDB(int senid, const char *value) {
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(EXIT_FAILURE);
    }


    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }

    char query[256];
    sprintf(query, "INSERT INTO SensorData (sensor_id, reading, timestamp) VALUES (%d, '%s', sysdate())", senid, value);

    if (mysql_query(conn, query) != 0) {
	fprintf(stderr, "MariaDB query execution failed: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
}

void sendToMQTT(char *topic, const char *value) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, MQTT_BROKER_ADDRESS, MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker: %d\n", rc);
        exit(EXIT_FAILURE);
    }
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)value;
    pubmsg.payloadlen = strlen(value);
    pubmsg.qos = 1;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, topic, &pubmsg, NULL);

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

int main() {
    char senval[2][10];
    signal(SIGCHLD, signalHandler);

    int pipefd[2];
    pid_t child_pid;
    struct message msg;

    // Create a message queue
    key_t key;
    int msgid;
    if ((key = ftok(".", 'a')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
    if ((msgid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

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

	        char *token = strtok((char *)receivedString, ":");
	       
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

	        for(int i=0; i < 2; i++) {
			// Save to MariaDB
			//saveToMariaDB(receivedString);
			saveToMariaDB((i+1), senval[i]);

			// Send to MQTT
			//sendToMQTT(receivedString);
			sendToMQTT(topics[i], senval[i]);
		}
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

