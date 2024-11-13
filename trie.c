#include "trie.h"

trie_node *create_trie_node(const char *topic)
{
    trie_node *root = (trie_node *)malloc(sizeof(trie_node));

    root->queue = create_queue(topic, "");
    root->is_end_of_topic = false;
    root->topic_part = strdup(topic);

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        root->children[i] = NULL;
    }

    return root;
}

trie_node *create_trie()
{
    trie_node *root = create_trie_node("*");
    free(root->queue);
    root->queue = NULL;

    return root;
}

void trie_insert_topic(trie_node *root, message message, const char *topic)
{
    trie_node *current_node = root;
    char *topic_copy = strdup(topic);
    char *part = strtok(topic_copy, ".");

    while (part)
    {
        bool found = false;

        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            if (current_node->children[i] &&
                strcmp(part, current_node->children[i]->topic_part) == 0)
            {
                current_node = current_node->children[i];
                found = true;
                break;
            }
        }

        if (!found)
        {
            for (int i = 0; i < MAX_CHILDREN; i++)
            {
                if (current_node->children[i] == NULL)
                {
                    current_node->children[i] = create_trie_node(part);
                    current_node = current_node->children[i];
                    break;
                }
            }
        }
        part = strtok(NULL, ".");
    }

    current_node->is_end_of_topic = true;
    current_node->queue = create_queue(topic, "");
    enqueue_message(current_node->queue, &message);

    free(topic_copy);
}

void trie_insert_queue(trie_node *root, const char *topic, queue *q)
{
}

trie_node *trie_search_topic(trie_node *root, const char *topic)
{
    trie_node *current = root;
    char *topic_copy = strdup(topic);
    char *part = strtok(topic_copy, ".");
 
    while (part)
    {
        bool found = false;

        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            if (current->children[i] &&
                strcmp(current->children[i]->topic_part, part) == 0)
            {
                current = current->children[i];
                found = true;
                break;
            }
        }

        if (!found)
        {
            free(topic_copy);
            return NULL;
        }

        part = strtok(NULL, ".");
    }

    free(topic_copy);
    return current->is_end_of_topic ? current : NULL;
}

void print_trie_helper(trie_node *node, char *buffer, int level)
{
    if (node == NULL)
    {
        return;
    }

    for (int i = 0; i < level * 4; i++)
    {
        printf(" ");
    }

    if (node->topic_part)
    {
        if (strcmp(node->topic_part, "*") != 0)
        {
            strcat(buffer, node->topic_part);
        }
        printf("|-- %s", node->topic_part);

        if (node->is_end_of_topic)
        {
            printf(" [Complet topic: %s]", buffer);
        }
        printf("\n");

        if (strcmp(node->topic_part, "*") != 0)
        {
            strcat(buffer, ".");
        }
    }

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (node->children[i])
        {
            print_trie_helper(node->children[i], buffer, level + 1);
        }
    }

    buffer[strlen(buffer) - (node->topic_part ? strlen(node->topic_part) + 1 : 0)] = '\0';
}

void print_trie(trie_node *root)
{
    printf("\nTrie:\n");
    char buffer[1024] = "";
    print_trie_helper(root, buffer, 0);
    printf("\n");
}

void free_trie(trie_node *root)
{
    if (root == NULL)
    {
        return;
    }

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (root->children[i])
        {
            free_trie(root->children[i]);
        }
    }

    free_queue(root->queue);
    free(root->topic_part);
    free(root);
}
