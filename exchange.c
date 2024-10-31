#include "exchange.h"

direct_exchange *create_direct_exchange()
{
    direct_exchange *exchange = (direct_exchange *)malloc(sizeof(direct_exchange));
    exchange->bindings = create_hash_table();
    return exchange;
}

void exch_insert_queue(direct_exchange *exchange, queue *q)
{
    ht_insert_queue(exchange->bindings, q);
}

// implementare algoritm echitabil
queue *exch_get_queue(direct_exchange *exchange, const char *binding_key)
{
    return ht_get_queue(exchange->bindings, binding_key);
}

void exch_insert_message(direct_exchange *exchange, message message, const char *binding_key)
{
    queue *q = exch_get_queue(exchange, binding_key);

    if (!q)
    {
        queue *new_queue = create_queue("", binding_key);
        enqueue_message(new_queue, &message);
        exch_insert_queue(exchange, new_queue);
    }
    else
    {
        enqueue_message(q, &message);
    }
}

void free_direct_exchange(direct_exchange *exchange)
{
    free_hash_table(exchange->bindings);
    free(exchange);
}