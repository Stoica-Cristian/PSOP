#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>

#include "cJSON.h"
#include "log.h"

//======================= Message ========================//

#define MAX_PAYLOAD_SIZE 8192

typedef enum
{
    MSG_EMPTY,
    MSG_INVALID,
    MSG_DIRECT,
    MSG_FANOUT,
    MSG_TOPIC
} message_type;

typedef struct
{
    message_type type;
    char payload[MAX_PAYLOAD_SIZE];
} message;

//======================= Queue ========================//

typedef struct q_node
{
    message message;
    struct q_node *next;
} q_node;

typedef struct queue
{
    char *q_topic;
    char *q_binding_key;
    int q_size;
    q_node *first_node;
    q_node *last_node;
    struct queue *next_queue;
} queue;

queue *create_queue(const char *topic, const char *binding_key);
void enqueue_message(queue *q, const message *msg);
message dequeue_message(queue *q);
void print_queue(queue *q);
void print_queues(queue *q);
void free_queue(queue *q);

#endif