#include "exchange.h"

//============================== Direct Exchange ==============================//

direct_exchange *create_direct_exchange()
{
    direct_exchange *exchange = (direct_exchange *)malloc(sizeof(direct_exchange));
    exchange->bindings = create_hash_table();
    return exchange;
}

void direct_exch_insert_queue(direct_exchange *exchange, queue *q)
{
    ht_insert_queue(exchange->bindings, q);
}

queue *direct_exch_get_queue(direct_exchange *exchange, const char *binding_key)
{
    return ht_get_queue(exchange->bindings, binding_key);
}

void direct_exch_insert_message(direct_exchange *exchange, message message, const char *binding_key)
{
    queue *q = direct_exch_get_queue(exchange, binding_key);

    if (!q)
    {
        queue *new_queue = create_queue("", binding_key);
        enqueue_message(new_queue, &message);
        direct_exch_insert_queue(exchange, new_queue);
    }
    else
    {
        enqueue_message(q, &message);
    }
}

void direct_exch_print_queues(direct_exchange *exchange)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        if (exchange->bindings->buckets[i] != NULL)
        {
            print_queues(exchange->bindings->buckets[i]->queues);
        }
    }
}

void free_direct_exchange(direct_exchange *exchange)
{
    free_hash_table(exchange->bindings);
    free(exchange);
}

//============================== Topic Exchange ==============================//

topic_exchange *create_topic_exchange()
{
    topic_exchange *exchange = (topic_exchange *)malloc(sizeof(topic_exchange));
    exchange->trie = create_trie();
    return exchange;
}

void topic_exch_insert_message(topic_exchange *exchange, message message, const char *topic)
{
    trie_insert_topic(exchange->trie, message, topic);
}

void topic_exch_insert_queue(topic_exchange *exchange, const char *topic, queue *q)
{
    trie_insert_queue(exchange->trie, topic, q);
}

queue *topic_exch_search_topic(topic_exchange *exchange, const char *topic)
{
    trie_node *node = trie_search_topic(exchange->trie, topic);
    return node ? node->queue : NULL;
}

void topic_exch_print_trie(topic_exchange *exchange)
{
    print_trie(exchange->trie);
}

void free_topic_exchange(topic_exchange *exchange)
{
    free_trie(exchange->trie);
    free(exchange);
}

//============================== Fanout Exchange ==============================//

