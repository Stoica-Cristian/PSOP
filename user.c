#include "user.h"

pthread_mutex_t user_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t find_user_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t register_user_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_users(user ***users)
{
    *users = (user **)malloc(MAX_USERS * sizeof(user *));

    for (int i = 0; i < MAX_USERS; i++)
    {
        (*users)[i] = NULL;
    }
}

void add_user(user **users, user *new_user)
{
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] == NULL)
        {
            users[i] = new_user;
            pthread_mutex_unlock(&user_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&user_mutex);
}

user *new_user(const char *username, const char *password)
{
    user *new_user = (user *)malloc(sizeof(user));
    generate_uid(&new_user->id);
    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';
    strncpy(new_user->password, password, sizeof(new_user->password) - 1);
    new_user->password[sizeof(new_user->password) - 1] = '\0';
    return new_user;
}

user *find_user(user **users, const char *username)
{
    pthread_mutex_lock(&find_user_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            if (strcmp(users[i]->username, username) == 0)
            {
                pthread_mutex_unlock(&find_user_mutex);
                return users[i];
            }
        }
    }
    pthread_mutex_unlock(&find_user_mutex);
    return NULL;
}

void remove_user(user **users, const char *username)
{
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL && strcmp(users[i]->username, username) == 0)
        {
            free(users[i]);
            users[i] = NULL;
            pthread_mutex_unlock(&user_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&user_mutex);
}

void set_user_password(user **users, const char *username, const char *password)
{
    pthread_mutex_lock(&user_mutex);
    user *found_user = find_user(users, username);
    if (found_user)
    {
        strncpy(found_user->password, password, sizeof(found_user->password) - 1);
        found_user->password[sizeof(found_user->password) - 1] = '\0';
    }
    pthread_mutex_unlock(&user_mutex);
}

void print_users(user **users)
{
    pthread_mutex_lock(&user_mutex);
    printf("Users:\n");
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            printf("\t%s\n", users[i]->username);
        }
    }
    printf("\n");
    pthread_mutex_unlock(&user_mutex);
}

void cleanup_users(user **users)
{
    pthread_mutex_lock(&user_mutex);
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            free(users[i]);
            users[i] = NULL;
        }
    }
    free(users);
    pthread_mutex_unlock(&user_mutex);
}

bool authenticate_user(user **users, const char *username, const char *password)
{
    pthread_mutex_lock(&user_mutex);
    user *found_user = find_user(users, username);

    if (found_user) 
    {
        bool result = strcmp(found_user->password, password) == 0;
        pthread_mutex_unlock(&user_mutex);
        return result;
    }

    pthread_mutex_unlock(&user_mutex);
    return false;
}

bool register_user(user **users, const char *username, const char *password)
{
    pthread_mutex_lock(&register_user_mutex);
    if (find_user(users, username) != NULL)
    {
        pthread_mutex_unlock(&register_user_mutex);
        return false;
    }

    user *new_user_instance = new_user(username, password);

    if (!new_user_instance)
    {
        pthread_mutex_unlock(&register_user_mutex);
        return false;
    }

    add_user(users, new_user_instance);
    pthread_mutex_unlock(&register_user_mutex);

    return true;
}