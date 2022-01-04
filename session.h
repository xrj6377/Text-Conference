#include "./constants.h"

typedef struct Session {
    char name[MAX_NAME_LENGTH];
    struct ClientInfo * clients;
    struct Session * next;
} Session;

typedef struct ClientInfo {
    char clientName[MAX_NAME_LENGTH];
    int clientId;
    int clientSockfd;
    struct ClientInfo * next;
} ClientInfo;