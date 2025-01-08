#include "user.h"

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
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] == NULL)
        {
            users[i] = new_user;
            print_users(users);
            return;
        }
    }
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
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            if (strcmp(users[i]->username, username) == 0)
            {
                return users[i];
            }
        }
    }
    return NULL;
}

void remove_user(user **users, const char *username)
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL && strcmp(users[i]->username, username) == 0)
        {
            free(users[i]);
            users[i] = NULL;
            return;
        }
    }
}

void set_user_password(user **users, const char *username, const char *password)
{
    user *found_user = find_user(users, username);
    if (found_user)
    {
        strncpy(found_user->password, password, sizeof(found_user->password) - 1);
        found_user->password[sizeof(found_user->password) - 1] = '\0';
    }
}

void set_user_id(user **users, const char *username, unique_id id)
{
    user *found_user = find_user(users, username);
    if (found_user)
    {
        found_user->id = id;
    }
}

void print_users(user **users)
{
    printf("Users:\n");
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            printf("\t%s\n", users[i]->username);
        }
    }
}

void cleanup_users(user **users)
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i] != NULL)
        {
            free(users[i]);
            users[i] = NULL;
        }
    }
    free(users);
}

bool authenticate_user(user **users, const char *username, const char *password)
{
    user *found_user = find_user(users, username);

    if (found_user) return strcmp(found_user->password, password) == 0;

    return false;
}

bool register_user(user **users, const char *username, const char *password)
{
    if (find_user(users, username) != NULL)
    {
        return false;
    }

    user *new_user_instance = new_user(username, password);

    if (!new_user_instance)
    {
        return false;
    }

    add_user(users, new_user_instance);

    return true;
}