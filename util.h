#include "./constants.h"
#include <strings.h>

enum type {
    LOGIN,
    LO_ACK,
    LO_NAK,
    LOGOUT,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    LEAVE_SESS_ACK,
    NEW_SESS,
    NS_ACK,
    NS_NAK,
    MESSAGE,
    QUERY_SESSION,
    QS_ACK,
    QUERY_USER,
    QU_ACK,
    DM,
    DM_ACK,
    DM_NAK,
    REG,
    REG_ACK,
    REG_NAK,
    UNKNOWN
};

typedef struct Packet {
    enum type type;
    int messageSize;
    int source;
    char message[MESSAGE_LENGTH];
} Packet;

void parseMessageToPacket(Packet * packet, char * buffer) {
    memset(packet, 0, sizeof (Packet));
    // make a copy of message since strtok will modify original string
    char bufferCopy[MESSAGE_LENGTH] = "";
    strcpy(bufferCopy, buffer);
    int offset = 0;

    // parsing senderPacket type
    char * token = strtok(buffer, ":");
    offset += strlen(token) + 1;
    unsigned long messageType = strtoul(token, NULL, 10);
    if (messageType >= 0 && messageType < UNKNOWN) {
        packet->type = messageType;
    }
    else {
        packet->type = UNKNOWN;
        bzero(buffer, MESSAGE_LENGTH);
        return;
    }

    // parsing length of message
    token = strtok(NULL, ":");
    offset += strlen(token) + 1;
    int messageSize = strtoul(token, NULL, 10);
    packet->messageSize = messageSize;

    // parsing id of client
    token = strtok(NULL, ":");
    offset += strlen(token) + 1;
    int sourceId = strtoul(token, NULL, 10);
    packet->source = sourceId;

    // parse message if there is one
    if (messageSize == 0) {
        bzero(buffer, MESSAGE_LENGTH);
        return;
    }
    strncpy(packet->message, bufferCopy + offset, messageSize);
    bzero(buffer, MESSAGE_LENGTH);
}

void parsePacketToMessage(Packet * packet, char * buffer) {
    bzero(buffer, MESSAGE_LENGTH);
    unsigned long offset = 0;

    // using offset to keep track of position of where we want the next part to be
    // type:size:clientid:message
    sprintf(buffer, "%d", packet->type);
    offset = strlen(buffer);
    sprintf(buffer + offset++, "%c", ':');
    sprintf(buffer + offset, "%d", packet->messageSize);
    offset = strlen(buffer);
    sprintf(buffer + offset++, "%c", ':');
    sprintf(buffer + offset, "%d", packet->source);
    offset = strlen(buffer);
    sprintf(buffer + offset++, "%c", ':');
    sprintf(buffer + offset, "%s", packet->message);
    memset(packet, 0, sizeof (Packet));
    bzero(packet->message, MESSAGE_LENGTH);
}

enum userAction {
    USER_CONNECT,
    USER_LOGIN,
    USER_LOGOUT,
    USER_JOIN_SESS,
    USER_LEAVE_SESS,
    USER_CREATE_SESS,
    USER_LIST,
    USER_ONLINELIST,
    USER_QUIT,
    USER_MESSAGE,
    USER_REG,
    USER_DM,
    USER_UNKNOWN
};

enum userAction parseUserCommand(char * input) {
    if (strlen(input) == 0) {
        printf("Empty command, please try again.\n");
        return USER_UNKNOWN;
    }

    // copy to preserve original input
    char inputCopy[MESSAGE_LENGTH] = "";
    strcpy(inputCopy, input);

    // determine whether it's a command or a message
    if (inputCopy[0] == '/') {
        char * token = strtok(inputCopy, " ");
        if (strcmp(token + 1, "connect") == 0) {
            return USER_CONNECT;
        }
        else if (strcmp(token + 1, "login") == 0) {
            return USER_LOGIN;
        }
        else if (strcmp(token + 1, "logout") == 0) {
            return USER_LOGOUT;
        }
        else if (strcmp(token + 1, "joinsession") == 0) {
            return USER_JOIN_SESS;
        }
        else if (strcmp(token + 1, "leavesession") == 0) {
            return USER_LEAVE_SESS;
        }
        else if (strcmp(token + 1, "createsession") == 0) {
            return USER_CREATE_SESS;
        }
        else if (strcmp(token + 1, "listsession") == 0) {
            return USER_LIST;
        }
        else if (strcmp(token + 1, "quit") == 0) {
            return USER_QUIT;
        }
        else if (strcmp(token + 1, "register") == 0) {
            return USER_REG;
        }
        else if (strcmp(token + 1, "listuser") == 0) {
            return USER_ONLINELIST;
        }
        else {
            return USER_DM;
        }
    }
    else {
        return USER_MESSAGE;
    }
}