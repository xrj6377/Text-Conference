#include "./constants.h"


typedef struct Account {
    int clientId;
    int sockfd;
    char userName[MAX_NAME_LENGTH];
    char password[MAX_NAME_LENGTH];
    bool loggedIn;
    struct Account * next;
} Account;