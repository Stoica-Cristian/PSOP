#ifndef TRIE_H
#define TRIE_H

#include "queue.h"

#define MAX_CHILDREN 256

typedef struct trie_node
{
    char *topic_part;
    queue *queues;
    struct trie_node *children[MAX_CHILDREN];
    bool is_end_of_topic;
} trie_node;

#endif