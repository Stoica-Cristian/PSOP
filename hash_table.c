#include "hash_table.h"

unsigned int hash(const char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % TABLE_SIZE;
}

hash_table *create_hash_table()
{
    hash_table *table = malloc(sizeof(hash_table));

    table->buckets = malloc(sizeof(hash_node *) * TABLE_SIZE);

    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table->buckets[i] = NULL;
    }

    return table;
}

void ht_insert_queue(hash_table *table, queue *q)
{
    unsigned int index = hash(q->q_binding_key);

    if (table->buckets[index] != NULL)
    {
        q->next_queue = table->buckets[index]->queues;
        table->buckets[index]->queues = q;
        table->buckets[index]->queue_count++;
    }
    else
    {
        hash_node *new_hash_node = malloc(sizeof(hash_node));

        new_hash_node->binding_key = strdup(q->q_binding_key);
        new_hash_node->queues = q;
        new_hash_node->current_queue_index = 0;
        new_hash_node->queue_count = 1;
        table->buckets[index] = new_hash_node;
    }
}


/**
 * Retrieves the queue associated with the given binding key from the hash table using a round-robin mechanism.
 *
 * @param table The hash table from which to retrieve the queue.
 * @param binding_key The key associated with the queue to retrieve.
 * @return A pointer to the queue associated with the given binding key, or NULL if the key is not found.
 */
queue *ht_get_queue(hash_table *table, const char *binding_key)
{
    unsigned int index = hash(binding_key);

    hash_node *node = table->buckets[index];

    if (node == NULL)
        return NULL;

    if (node->queue_count == 0)
    {
        log_error("[ht_get_queue(hash_table*, const char*)] : Queue count is 0");
        return NULL;
    }

    if (node->queue_count == 1)
    {
        return node->queues;
    }

    queue *q = node->queues;
    int current_index = node->current_queue_index % node->queue_count;

    for (int i = 0; i < current_index; i++)
    {
        q = q->next_queue;
    }

    node->current_queue_index = (node->current_queue_index + 1) % node->queue_count;

    return q;
}


void free_hash_table(hash_table *table)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        queue *q = table->buckets[i]->queues;
        while (q != NULL)
        {
            queue *current_queue = q;
            q = q->next_queue;
            free_queue(current_queue);
        }
        free(table->buckets[i]);
    }
    free(table->buckets);
    free(table);
}
