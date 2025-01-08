#ifndef TRIE_H
#define TRIE_H

#include "queue.h"
#include "user.h"

#define MAX_CHILDREN 256

extern user **users;

typedef struct trie_node
{
    char *topic_part;
    queue *queue;
    struct trie_node *children[MAX_CHILDREN];
    bool is_end_of_topic;
    user **subscribers;
} trie_node;

trie_node *create_trie_node(const char *topic);
trie_node *create_trie();
void trie_insert_topic(trie_node *root, message message, const char *topic);
void trie_insert_queue(trie_node *root, const char *topic, queue *q);
trie_node *trie_search_topic(trie_node *root, const char *topic);
void print_trie(trie_node *root);
void print_trie_helper(trie_node *node, char *buffer, int level);
void free_trie(trie_node *root);

void trie_insert_subscriber(trie_node *root, const char *topic, user *subscriber);
void trie_print_subscribers(trie_node *root);

#endif