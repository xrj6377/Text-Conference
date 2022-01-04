#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include "./../util.h"

// action handler
void processUserConnect(char * userInput);
void processUserLogin(char * userInput);
void processUserRegister(char * userInput);
void processUserLogout(bool isQuit);

void processNewSession(char * userInput);
void processJoinSess(char * userInput);
void processLeaveSess();

void processSessionList();
void processUserList();

void processMessage(char * userInput);
void processUserDM(char * userInput);

// helper functions
void * serverListener();
void listQueryResult(char * message);
size_t listenFromServer();
int checkAndCopy(char * token, char * dest);

// global variables
bool loggedIn = false;                  // log in status of the client
bool inSession = false;                 // check client in session or not
int sockfd = -1;
int userId = -1;                        // unique id assigned by server
char sendingBuffer[MESSAGE_LENGTH] = "";
char clientSession[MESSAGE_LENGTH] = "";
pthread_t listenerThread;                   // thread for session connection
pthread_mutex_t MUTEX = PTHREAD_MUTEX_INITIALIZER; // mutex
Packet senderPacket;

int main() {
    char userInput[MESSAGE_LENGTH] = "";
    char temp;
    while (1) {
        strcpy(userInput, "");
        scanf("%[^\n]s", userInput);
        scanf("%c", &temp);

        // can only log out, quit, leavesession or send message if the client is in a session
        enum userAction action = parseUserCommand(userInput);

        switch (action) {
            case USER_CONNECT:
                processUserConnect(userInput);
                break;
            case USER_LOGIN:
                processUserLogin(userInput);
                break;
            case USER_LOGOUT:
                if (inSession) {
                    processLeaveSess();
                }
                processUserLogout(false);
                break;
            case USER_CREATE_SESS:
                processNewSession(userInput);
                break;
            case USER_LIST:
                processSessionList();
                break;
            case USER_ONLINELIST:
                processUserList();
                break;
            case USER_JOIN_SESS:
                processJoinSess(userInput);
                break;
            case USER_LEAVE_SESS:
                processLeaveSess();
                break;
            case USER_MESSAGE:
                processMessage(userInput);
                break;
            case USER_QUIT:
                if (inSession) {
                    processLeaveSess();
                }
                if (loggedIn) {
                    processUserLogout(true);
                }
                goto userQuit;
            case USER_REG:
                processUserRegister(userInput);
                break;
            case USER_DM:
                processUserDM(userInput);
                break;
            case USER_UNKNOWN:
                printf("Unknown command, please try again.\n");
                break;
        }
    }

userQuit:
    if (sockfd != -1) {
        close(sockfd);
    }
    return 0;
}

void processUserConnect(char * userInput) {
    if (sockfd != -1) {
        printf("Already connected to the server.\n");
        return;
    }

    char serverIP[16] = "";
    char serverPort[6] = "";
    char * token = strtok(userInput, " ");
    // skip command
    token = strtok(NULL, " ");

    int flag = 1;
    flag &= checkAndCopy(token, serverIP);
    token = strtok(NULL, " ");
    flag &= checkAndCopy(token, serverPort);

    if (flag == 0) {
        printf("Failed to parse connect command, command is not complete.\n");
        return;
    }
    else {
        // never established connection to server
        unsigned long portNum = strtoul(serverPort, NULL, 10);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in sa_in;
        memset(&sa_in, 0, sizeof sa_in);
        sa_in.sin_family = AF_INET;
        sa_in.sin_port = htons(portNum);
        sa_in.sin_addr.s_addr = inet_addr(serverIP);
        if (connect(sockfd, (struct sockaddr *)&sa_in, (socklen_t)sizeof sa_in) == -1) {
            printf("Failed to establish connection with server.\n");
            close(sockfd);
            sockfd = -1;
            return;
        } else {
            // create separate thread to listen from server all the time
            pthread_create(&listenerThread, NULL, serverListener, NULL);
            printf("Connection with server established.\n");
        }
    }
}

void processUserLogin(char * userInput) {
    if (sockfd == -1) {
        printf("Not connected to the server yet.\n");
        return;
    }

    if (loggedIn) {
        printf("User already logged in.\n");
        return;
    }
    char userName[128] = "";
    char userPassword[128] = "";
    memset(&senderPacket, 0, sizeof (Packet));

    char * token = strtok(userInput, " ");

    // skip command
    token = strtok(NULL, " ");

    int flag = 1;

    // parse log in information
    // format: username password ip port
    flag &= checkAndCopy(token, userName);
    token = strtok(NULL, " ");
    flag &= checkAndCopy(token, userPassword);

    if (flag == 0) {
        printf("Failed to parse login command, command is not complete.\n");
        return;
    }
    else {
        senderPacket.type = LOGIN;
        strcpy(senderPacket.message, userName);
        strcat(senderPacket.message, " ");
        strcat(senderPacket.message, userPassword);
        senderPacket.messageSize = strlen(senderPacket.message);

        // initially, client doesn't know its id, server will tell client if login was successful
        senderPacket.source = -1;

        parsePacketToMessage(&senderPacket, sendingBuffer);

        if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
            printf("Failed to send message to server, please try again.\n");
            return;
        }
    }
}

void processUserRegister(char * userInput) {
    if (sockfd == -1) {
        printf("Not connected to the server yet.\n");
        return;
    }

    if (loggedIn) {
        printf("User already logged in.\n");
        return;
    }

    char userName[MAX_NAME_LENGTH] = "";
    char userPassword[MAX_NAME_LENGTH] = "";
    char * token = strtok(userInput, " ");
    memset(&senderPacket, 0, sizeof (Packet));

    // skip command
    token = strtok(NULL, " ");

    int flag = 1;
    flag &= checkAndCopy(token, userName);
    token = strtok(NULL, " ");
    flag &= checkAndCopy(token, userPassword);

    if (flag == 0) {
        printf("Failed to parse register command, command is not complete.\n");
        return;
    }
    else {
        senderPacket.source = -1;
        senderPacket.type = REG;
        strcpy(senderPacket.message, userName);
        strcat(senderPacket.message, " ");
        strcat(senderPacket.message, userPassword);
        senderPacket.messageSize = strlen(senderPacket.message);

        parsePacketToMessage(&senderPacket, sendingBuffer);
        if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
            printf("Failed to send message to server, please try again.\n");
            return;
        }
    }
}

void processNewSession(char * userInput) {
    if (!loggedIn) {
        printf("Can't create session when not logged in.\n");
        return;
    }

    char * token = strtok(userInput, " ");
    token = strtok(NULL, " ");

    if (token == NULL) {
        printf("Didn't provide name for new session.\n");
        return;
    }

    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = NEW_SESS;
    senderPacket.source = userId;
    senderPacket.messageSize = strlen(token);
    strcpy(senderPacket.message, token);

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processUserLogout(bool isQuit) {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }

    // send message to the server informing the client is about to exit
    // handle the default values for the user
    senderPacket.type = isQuit ? EXIT : LOGOUT;
    senderPacket.messageSize = 0;
    senderPacket.source = userId;
    parsePacketToMessage(&senderPacket, sendingBuffer);
    send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0);
    if (isQuit) {
        close(sockfd);
        sockfd = -1;
    }
    pthread_mutex_lock(&MUTEX);
    userId = -1;
    loggedIn = false;
    pthread_mutex_unlock(&MUTEX);
    printf("You are logged out.\n");
}

void processSessionList() {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }

    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = QUERY_SESSION;
    senderPacket.source = userId;
    senderPacket.messageSize = 0;

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processUserList() {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }

    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = QUERY_USER;
    senderPacket.source = userId;
    senderPacket.messageSize = 0;

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processJoinSess(char * userInput) {
    // must log in and can only join one session
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }
    else if (inSession) {
        printf("Already in session %s, please leave current session to switch.\n", clientSession);
        return;
    }

    char * token = strtok(userInput, " ");

    token = strtok(NULL, " ");
    if (token == NULL) {
        printf("Didn't specify which clientSession to join, please try again.\n");
        return;
    }

    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = JOIN;
    senderPacket.source = userId;
    strcpy(senderPacket.message, token);
    senderPacket.messageSize = strlen(senderPacket.message);

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processLeaveSess() {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }
    else if (!inSession) {
        printf("Not in any session yet.\n");
        return;
    }

    pthread_cancel(listenerThread);

    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = LEAVE_SESS;
    senderPacket.source = userId;
    strcpy(senderPacket.message, clientSession);
    senderPacket.messageSize = strlen(senderPacket.message);

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processMessage(char * userInput) {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }
    else if (!inSession) {
        printf("Not in any session yet.\n");
        return;
    }

    // process the message to be sent together with userid, session name
    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = MESSAGE;
    senderPacket.source = userId;
    int offset = strlen(clientSession);
    strcpy(senderPacket.message, clientSession);
    strcat(senderPacket.message + offset++, " ");
    strcat(senderPacket.message + offset, userInput);
    senderPacket.messageSize = strlen(senderPacket.message);

    parsePacketToMessage(&senderPacket, sendingBuffer);
    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void processUserDM(char * userInput) {
    if (!loggedIn) {
        printf("User not logged in.\n");
        return;
    }

    // target is the target user to receive the message
    char packetMessageCopy[MESSAGE_LENGTH] = "";
    char target[MESSAGE_LENGTH] = "";
    strcpy(packetMessageCopy, userInput + 1);
    char * token = strtok(userInput + 1, " ");
    strcpy(target, token);
    token = strtok(NULL, " ");


    if (token == NULL) {
        // format for user dm should be
        // user: message
        printf("Didn't provide valid message\n");
        return;
    }

    // senderPacket the dm message to the server to contact the target user
    memset(&senderPacket, 0, sizeof (Packet));
    senderPacket.type = DM;
    senderPacket.source = userId;
    senderPacket.messageSize = strlen(packetMessageCopy);
    strcpy(senderPacket.message, packetMessageCopy);

    parsePacketToMessage(&senderPacket, sendingBuffer);

    if (send(sockfd, sendingBuffer, MESSAGE_LENGTH, 0) == -1) {
        printf("Failed to send message to server, please try again.\n");
        return;
    }
}

void * serverListener() {
    char receivingBuffer[MESSAGE_LENGTH] = "";
    Packet receivingPacket;

    while (recv(sockfd, receivingBuffer, MESSAGE_LENGTH, 0) > 0) {
        parseMessageToPacket(&receivingPacket, receivingBuffer);
        switch (receivingPacket.type) {
            case LO_ACK:
                pthread_mutex_lock(&MUTEX);
                userId = strtoul(receivingPacket.message, NULL, 10);
                loggedIn = true;
                pthread_mutex_unlock(&MUTEX);
                printf("Login was successful.\n");
                break;
            case LO_NAK:
                printf("Login attempt failed, please try again.\n");
                break;
            case REG_ACK:
                printf("Account registered.\n");
                break;
            case REG_NAK:
                printf("Failed to create account.\n");
                break;
            case NS_ACK:
                printf("Session created successfully.\n");
                break;
            case NS_NAK:
                printf("Failed to create session, please try again.\n");
                break;
            case QS_ACK:
                listQueryResult(receivingPacket.message);
                break;
            case QU_ACK:
                listQueryResult(receivingPacket.message);
                break;
            case JN_ACK:
                pthread_mutex_lock(&MUTEX);
                bzero(clientSession, MESSAGE_LENGTH);
                strcpy(clientSession, receivingPacket.message);
                inSession = true;
                pthread_mutex_unlock(&MUTEX);
                printf("Successfully joined session %s.\n", clientSession);
                break;
            case JN_NAK:
                printf("Failed to join session: %s\n", receivingPacket.message);
                break;
            case LEAVE_SESS_ACK:
                printf("Left session %s.\n", receivingPacket.message);
                pthread_mutex_lock(&MUTEX);
                inSession = false;
                memset(clientSession, 0, MESSAGE_LENGTH);
                pthread_mutex_unlock(&MUTEX);
                break;
            case DM_ACK:
                printf("DM sent successfully.\n");
                break;
            case DM_NAK:
                printf("%s\n", receivingPacket.message);
                break;
            case MESSAGE:
                printf("%s\n", receivingPacket.message);
                break;
        }
    }
    return NULL;
}

void listQueryResult(char * message) {
    char * token = strtok(message, " ");

    // print the list of sessions and users line by line
    while (token != NULL) {
        printf("%s\n", token);
        token = strtok(NULL, " ");
    }
}

size_t listenFromServer() {
    bzero(sendingBuffer, MESSAGE_LENGTH);
    size_t recvResult = recv(sockfd, sendingBuffer, MESSAGE_LENGTH, 0);

    if (recvResult == -1) {
        printf("Failed to receive message from server, please try again.\n");
        bzero(sendingBuffer, MESSAGE_LENGTH);
        return recvResult;
    }
    else if (recvResult == 0) {
        close(sockfd);
        sockfd = -1;
        loggedIn = false;
        inSession = false;
        bzero(clientSession, MESSAGE_LENGTH);
        printf("Connection with server is closed, please try to login again.\n");
        bzero(sendingBuffer, MESSAGE_LENGTH);
        return recvResult;
    }
    else {
        parseMessageToPacket(&senderPacket, sendingBuffer);
        return recvResult;
    }
}

int checkAndCopy(char * token, char * dest) {
    if (token == NULL) {
        return 0;
    }
    else {
        strcpy(dest, token);
        return 1;
    }
}