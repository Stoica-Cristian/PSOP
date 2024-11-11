#include "queue.h"

queue *create_queue(const char *topic, const char *binding_key)
{
    queue *q = (queue *)malloc(sizeof(queue));

    q->q_topic = strdup(topic);
    q->q_binding_key = strdup(binding_key);

    q->q_size = 0;

    q->first_node = NULL;
    q->last_node = NULL;
    q->next_queue = NULL;
    return q;
}

void enqueue_message(queue *q, const message *msg)
{
    q_node *new_node = (q_node *)malloc(sizeof(q_node));
    new_node->message = *msg;
    new_node->next = NULL;

    if (q->last_node == NULL)
    {
        q->first_node = new_node;
    }
    else
    {
        q->last_node->next = new_node;
    }

    q->last_node = new_node;
    q->q_size++;
}

message dequeue(queue *q)
{
    message msg;
    if (q->first_node == NULL)
    {
        msg.type = MSG_INVALID;
        msg.payload[0] = '\0';
        return msg;
    }

    q_node *temp = q->first_node;
    msg = temp->message;
    q->first_node = q->first_node->next;

    if (q->first_node == NULL)
    {
        q->last_node = NULL;
    }

    free(temp);
    q->q_size--;
    return msg;
}

void free_queue(queue *q)
{
    q_node *current = q->first_node;

    while (current != NULL)
    {
        q_node *temp = current;
        current = current->next;
        free(temp);
    }

    free(q->q_topic);
    free(q->q_binding_key);
    free(q);
}

void print_queue(queue *q)
{
    printf("\n");
    if (strlen(q->q_topic) > 0)
        printf("Queue Topic: %s\n", q->q_topic);

    if (strlen(q->q_binding_key) > 0)
        printf("Queue Binding Key: %s\n", q->q_binding_key);

    printf("Queue Size: %d\n", q->q_size);

    q_node *current = q->first_node;
    int index = 1;
    while (current != NULL)
    {
        printf("\nMessage %d: %s", index++, current->message.payload);
        
        current = current->next;
    }
    printf("\n\n");
}

void print_queues(queue *q)
{
    queue* current_queue = q;
    while(current_queue)
    {
        print_queue(current_queue);
        current_queue = current_queue->next_queue;
    }
}
