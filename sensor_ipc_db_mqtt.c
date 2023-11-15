#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <wiringPi.h>
#include <mariadb/mysql.h>
#include <mqtt/MQTTClient.h>

#define MAX_TEXT 512
#define GPIO_PIN 4  // GPIO pin number, change it according to your setup

// MariaDB Connection parameters
#define DB_HOST "localhost"
#define DB_USER "your_db_user"
#define DB_PASSWORD "your_db_password"
#define DB_NAME "your_db_name"

// MQTT Connection parameters
#define MQTT_BROKER_ADDRESS "tcp://mqtt.eclipse.org:1883"
#define MQTT_CLIENT_ID "temperature_sensor_client"
#define MQTT_TOPIC "temperature_data"

// Define a message structure
struct message {
    long msg_type;
    char msg_text[MAX_TEXT];
};

int msgid;  // Message queue ID
MYSQL *conn; // MariaDB connection

// Function to handle cleanup on program termination
void cleanup() {
    // Close MariaDB connection
    mysql_close(conn);

    // Remove the message queue
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    printf("Cleanup complete. Exiting.\n");
}

// Signal handler for cleanup on Ctrl+C
void handleSignal(int signum) {
    if (signum == SIGINT) {
        cleanup();
        exit(EXIT_SUCCESS);
    }
}

// Function to create a message queue
int createMessageQueue() {
    key_t key;

    // Generate a unique key
    if ((key = ftok(".", 'a')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    // Create a message queue
    int msgid;
    if ((msgid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    return msgid;
}

// MariaDB initialization
void initMariaDB() {
    conn = mysql_init(NULL);

    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(EXIT_FAILURE);
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASSWORD, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }
}

// MariaDB query execution
void executeMariaDBQuery(const char *query) {
    if (mysql_query(conn, query) != 0) {
        fprintf(stderr, "MariaDB query execution failed: %s\n", mysql_error(conn));
    }
}

// MQTT message callback
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("MQTT Message Arrived: %s\n", (char *)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// Consumer function
void temperatureDataConsumer() {
    struct message msg;

    // MQTT Initialization
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, MQTT_BROKER_ADDRESS, MQTT_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to set MQTT callbacks: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Receive the message
        if (msgrcv(msgid, (void*)&msg, MAX_TEXT, 1, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        // Save data to MariaDB
        char query[MAX_TEXT];
        sprintf(query, "INSERT INTO temperature_data (value) VALUES ('%s')", msg.msg_text);
        executeMariaDBQuery(query);

        // Publish data to MQTT
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = msg.msg_text;
        pubmsg.payloadlen = strlen(msg.msg_text);
        pubmsg.qos = 1;
        pubmsg.retained = 0;
        MQTTClient_publishMessage(client, MQTT_TOPIC, &pubmsg, NULL);

        printf("Consumer received: %s\n", msg.msg_text);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}

int main() {
    // Set up Ctrl+C signal handler
    signal(SIGINT, handleSignal);

    // Initialize MariaDB
    initMariaDB();

    // Create a message queue
    msgid = createMessageQueue();

    // Fork a process for the consumer
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process (consumer)
        temperatureDataConsumer();
    } else {
        // Parent process (producer)
        if (wiringPiSetup() == -1) {
            perror("wiringPiSetup");
            exit(EXIT_FAILURE);
        }

        pinMode(GPIO_PIN, INPUT);

        while (1) {
            // Simulate reading temperature from GPIO pin
            int sensorValue = digitalRead(GPIO_PIN);
            char temperature[MAX_TEXT];
            sprintf(temperature, "Temperature: %d", sensorValue);

            // Set the message type
            struct message msg;
            msg.msg_type = 1;
            strcpy(msg.msg_text, temperature);

            // Send the message
            if (msgsnd(msgid, (void*)&msg, MAX_TEXT, 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            printf("Producer sent: %s\n", msg.msg_text);
            sleep(1);
        }

        // Wait for the consumer to finish
        wait(NULL);

        // Perform cleanup
        cleanup();
    }

    return 0;
}
