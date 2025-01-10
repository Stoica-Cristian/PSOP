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

    initialize_users(&root->subscribers);

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

    if (current_node->queue == NULL)
    {
        current_node->queue = create_queue(topic, "");
        enqueue_message(current_node->queue, &message);
    }
    else
    {
        enqueue_message(current_node->queue, &message);
    }

    free(topic_copy);

    if (current_node->subscribers)
    {
        for (int i = 0; current_node->subscribers[i] != NULL; i++)
        {
            user *subscriber = current_node->subscribers[i];

            if (subscriber->socketFD > 0)
            {
                cJSON *json_payload = cJSON_CreateObject();
                cJSON_AddStringToObject(json_payload, "type", "topic");
                cJSON_AddStringToObject(json_payload, "topic", topic);
                cJSON_AddStringToObject(json_payload, "payload", message.payload);

                char *json_payload_str = cJSON_PrintUnformatted(json_payload);
                packet response_packet = create_packet(PKT_CONSUMER_RESPONSE, json_payload_str);

                free(json_payload_str);
                cJSON_Delete(json_payload);

                send_packet(subscriber->socketFD, &response_packet);

                log_info("Message sent to subscriber: %s\n", subscriber->username);
            }
        }
    }
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
            if (node->queue)
            {
                printf("\n");

                q_node *current = node->queue->first_node;
                int index = 1;
                while (current != NULL)
                {
                    for (int i = 0; i < level * 6; i++)
                    {
                        printf(" ");
                    }

                    printf("Message %d: %s", index++, current->message.payload);

                    if (current->next)
                    {
                        printf("\n");
                    }
                    current = current->next;
                }
            }
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
    if (root == NULL)
    {
        return;
    }

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

void trie_insert_subscriber(trie_node *root, const char *topic, user *subscriber)
{
    trie_node *node = trie_search_topic(root, topic);

    if (node)
    {
        if (node->subscribers)
        {
            for (int i = 0; node->subscribers[i] != NULL; i++)
            {
                if (strcmp(node->subscribers[i]->username, subscriber->username) == 0)
                {
                    log_info("User %s is already subscribed to topic: %s", subscriber->username, topic);
                    return;
                }
            }
        }

        add_user(node->subscribers, subscriber);
        log_info("User %s subscribed to topic: %s", subscriber->username, topic);
    }
    else
    {
        log_info("Topic %s not found.", topic);
    }

    trie_print_subscribers(root);
}

void traverse_and_print(trie_node *node, char *buffer, int level)
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
            if (node->subscribers)
            {
                printf("\n");
                
                for (int i = 0; i < level * 6; i++)
                {
                    printf(" ");
                }


                for (int i = 0; node->subscribers[i] != NULL; i++)
                {
                    printf("%d: %s", i + 1, node->subscribers[i]->username);

                    if (node->subscribers[i + 1] != NULL)
                    {
                        printf("\n");
                    }
                }
            }
            else
            {
                // printf("No subscribers\n");
            }
        }
        printf("\n");

        if (strcmp(node->topic_part, "*") != 0)
        {
            strcat(buffer, ".");
        }
    }

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (node->children[i] != NULL)
        {
            traverse_and_print(node->children[i], buffer, level + 1);
        }
    }

    buffer[strlen(buffer) - (node->topic_part ? strlen(node->topic_part) + 1 : 0)] = '\0';
}

void trie_print_subscribers(trie_node *root)
{
    if (root == NULL)
    {
        return;
    }

    printf("\nSubscribers:\n");
    char topic_buffer[1024] = "";

    traverse_and_print(root, topic_buffer, 0);
    printf("\n");
}
