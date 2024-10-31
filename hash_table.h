#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "queue.h"

#define TABLE_SIZE 100

typedef struct
{
    char *binding_key;
    queue *queues;
} hash_node;

typedef struct
{
    hash_node **buckets;
} hash_table;

unsigned int hash(const char *str);

hash_table *create_hash_table();
void ht_insert_queue(hash_table *table, queue *q);
queue *ht_get_queue(hash_table *table, const char *binding_key);
void free_hash_table(hash_table *table);

#endif