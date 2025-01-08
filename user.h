#include "packet.h"

#define MAX_USERS 100

typedef struct user
{
    unique_id id;
    char username[50];
    char password[50];
    // struct user *next;
    int socketFD;
} user;

void initialize_users(user ***users);
void add_user(user **users, user *new_user);
user* new_user(const char *username, const char *password);
user* find_user(user **users, const char *username);
void remove_user(user **users, const char *username);
void set_user_password(user **users, const char *username, const char *password);
void set_user_id(user **users, const char *username, unique_id id);

void print_users(user **users);
void cleanup_users(user **users);

bool authenticate_user(user **users, const char *username, const char *password);
bool register_user(user **users, const char *username, const char *password);