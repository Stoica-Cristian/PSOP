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
    trie_node *trie;
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
void direct_exch_insert_queue(direct_exchange *exchange, queue *q);
queue *direct_exch_get_queue(direct_exchange *exchange, const char *binding_key);
void direct_exch_insert_message(direct_exchange *exchange, message message, const char *binding_key);
void direct_exch_print_queues(direct_exchange *exchange);
void free_direct_exchange(direct_exchange *exchange);

// Topic exchange

topic_exchange *create_topic_exchange();
void topic_exch_insert_topic(topic_exchange *exchange, const char *topic);
void topic_exch_insert_queue(topic_exchange *exchange, const char *topic, queue *q);
queue *topic_exch_search_topic(topic_exchange *exchange, const char *topic);
void topic_exch_print_trie(topic_exchange *exchange);
void free_topic_exchange(topic_exchange *exchange);

#endif
