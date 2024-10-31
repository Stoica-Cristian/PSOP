#ifndef EXCHANGE_H
#define EXCHANGE_H

#include "hash_table.h"
#include "trie.h"

typedef struct direct_exchange
{
    hash_table *bindings;
} direct_exchange;

typedef struct fanout_exchange
{
    queue *queues;
} fanout_exchange;

typedef struct topic_exchange
{
    trie_node *trie_root;
} topic_exchange;

typedef enum
{
    EXCH_INVALID,
    EXCH_DIRECT,
    EXCH_FANOUT,
    EXCH_TOPIC
} exchange_type;

typedef struct exchange
{
    exchange_type type;
    union
    {
        direct_exchange direct;
        fanout_exchange fanout;
        topic_exchange topic;
    };
} exchange;

// Direct exchange

direct_exchange *create_direct_exchange();
void exch_insert_queue(direct_exchange *exchange, queue *q);
queue *exch_get_queue(direct_exchange *exchange, const char *binding_key);
void exch_insert_message(direct_exchange* exchange, message message, const char* binding_key);
void free_direct_exchange(direct_exchange *exchange);

#endif
