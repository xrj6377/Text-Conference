#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include "./../util.h"
#include "./../session.h"
#include "./../account.h"

pthread_mutex_t MUTEX = PTHREAD_MUTEX_INITIALIZER;

//char * USER_NAME[NUM_CLIENT_ACCOUNT] = {"user0", "user1", "user2", "user3"};
//char * USER_PASSWORD[NUM_CLIENT_ACCOUNT] = {"user0", "user1", "user2", "user3"};
//bool LOGIN_STATUS[NUM_CLIENT_ACCOUNT] = {false, false, false, false};
Session * sessions = NULL;
Session * sessionTail = NULL;
Account * accounts = NULL;
Account * accountTail = NULL;
int newAccountIndex = 0;
int session_count = 0;

// thread function to handle all client
void * processClient(void * clientSockFD);

void processRegister(int cSokFD, Packet * packet, char * buffer);
void processLogin(int cSokFD, Packet * packet, char * buffer, Account ** acctPtr);
void processLogout(Packet * packet, Account ** acctPtr);
void processNewSession(int cSokFD, Packet * packet, char * buffer);
void processSessionQuery(int cSokFD, Packet * packet, char * buffer);
void processUserQuery(int cSokFD, Packet * packet, char * buffer);
void processJoin(int cSokFD, Packet * packet, char * buffer, Account * userAccount);
void processLeave(int cSokFD, Packet * packet, char * buffer);
void processNewMessage(int cSokFD, Packet * packet, char * buffer);
void processDM(int cSokFD, Packet * packet, char * buffer);

int main(int argc, char* argv[]) {
    // return if port number not specified
    if (argc < 2) {
        printf("TCP listen port is missing.\n");
        return 1;
    }

    // convert port number to ul
    char* portStr = argv[1];
    unsigned long port;
    port = strtoul(portStr, NULL, 10);
    int sockfd;

    // set up address and port number for server to listen on
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    // initialize socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // bind socket to the predefined address and check if we're successful.
    int bindResult = bind(sockfd, (struct sockaddr*) &sa, sizeof sa);
    if (bindResult == -1) {
        printf("Server socket failed to bind, exiting.\n");
        close(sockfd);
        return 1;
    }
    else {
        printf("Bind successful.\n");
    }

    // mark socket as listener
    int listener = listen(sockfd, 3);
    if (listener == -1) {
        printf("Failed to mark socket as listener.\n");
        close(sockfd);
        return 1;
    }
    else {
        printf("Socket has been marked as listener.\n");
    }

    FILE * fp = fopen("./accounts.txt", "a+");
    char accountBuffer[MAX_NAME_LENGTH] = "";
    while (fgets(accountBuffer, MAX_NAME_LENGTH, fp) != NULL) {
        Account * a = malloc(sizeof (Account));
        strcpy(a->userName, accountBuffer);
        a->userName[strlen(a->userName) - 1] = '\0';
        memset(accountBuffer, 0, sizeof accountBuffer);
        if (fgets(accountBuffer, MAX_NAME_LENGTH, fp) == NULL) {
            printf("Account Info is missing.\n");
            free(a);
            break;
        }
        strcpy(a->password, accountBuffer);
        a->password[strlen(a->password) - 1] = '\0';
        a->clientId = newAccountIndex++;
        a->loggedIn = false;
        a->next = NULL;
        if (accounts == NULL) {
            accounts = a;
            accountTail = accounts;
        }
        else {
            accountTail->next = a;
            accountTail = accountTail->next;
        }
    }
    fclose(fp);

    // initialize array to store client socketFD and thread id.
    int clientIdx = 0;
    pthread_t tid[MAX_CONNECTIONS_ALLOWED];
    int cSocFD[MAX_CONNECTIONS_ALLOWED];
    struct sockaddr_in sa_in;
    socklen_t sl = sizeof sa_in;
    memset(&sa_in, 0, sl);
    for (int i = 0; i < MAX_CONNECTIONS_ALLOWED; i++) {
        cSocFD[i] = -1;
    }

    printf("Waiting for clients to connect, maximum %d.\n", MAX_CONNECTIONS_ALLOWED);
    while (clientIdx < MAX_CONNECTIONS_ALLOWED) {
        cSocFD[clientIdx] = accept(sockfd, (struct sockaddr *) &sa_in, &sl);
        if (cSocFD[clientIdx] != -1) {
            // client connected, create new thread to handle new client.
            printf("Client %d connected with sockFD %d.\n", clientIdx, cSocFD[clientIdx]);
            pthread_create(&tid[clientIdx], NULL, processClient, &cSocFD[clientIdx]);
            clientIdx++;
        }
    }
    close(sockfd);
    // wait for all client thread to terminate
    for (int i = 0; i < MAX_CONNECTIONS_ALLOWED; i++) {
        pthread_join(tid[i], NULL);
    }

    // clean up
    for (int i = 0; i < MAX_CONNECTIONS_ALLOWED; i++) {
        close(cSocFD[i]);
    }

    Session * sTemp = NULL;
    while (sessions != NULL) {
        sTemp = sessions->next;
        free(sessions);
        sessions = sTemp;
    }

    Account * aTemp = NULL;
    while (accounts != NULL) {
        aTemp = accounts->next;
        free(accounts);
        accounts = aTemp;
    }
}

void * processClient(void * clientSockFD) {
    int cSokFD = *(int *) clientSockFD;
    char buffer[MESSAGE_LENGTH] = "";
    Packet packet;
    Account * userAccount = NULL;
    bool shouldExit = false;

    while (recv(cSokFD, buffer, MESSAGE_LENGTH, 0) > 0) {
        parseMessageToPacket(&packet, buffer);

        pthread_mutex_lock(&MUTEX);
        // read the type of action
        switch (packet.type) {
            case REG:
                processRegister(cSokFD, &packet, buffer);
                break;
            case LOGIN:
                processLogin(cSokFD, &packet, buffer, &userAccount);
                break;
            case LOGOUT:
                processLogout(&packet, &userAccount);
                break;
            case EXIT:
                processLogout(&packet, &userAccount);
                shouldExit = true;
                break;
            case JOIN:
                processJoin(cSokFD, &packet, buffer, userAccount);
                break;
            case LEAVE_SESS:
                processLeave(cSokFD, &packet, buffer);
                break;
            case NEW_SESS:
                processNewSession(cSokFD, &packet, buffer);
                break;
            case MESSAGE:
                processNewMessage(cSokFD, &packet, buffer);
                break;
            case DM:
                processDM(cSokFD, &packet, buffer);
                break;
            case QUERY_SESSION:
                processSessionQuery(cSokFD, &packet, buffer);
                break;
            case QUERY_USER:
                processUserQuery(cSokFD, &packet, buffer);
                break;
            default:
                printf("Unknown senderPacket type.\n");
                break;
        }
        pthread_mutex_unlock(&MUTEX);

        if (shouldExit) {
            break;
        }
    }


    if (userAccount != NULL) {
        pthread_mutex_lock(&MUTEX);
        userAccount->loggedIn = false;
        pthread_mutex_unlock(&MUTEX);
    }
    close(cSokFD);
    return NULL;
}

void processRegister(int cSokFD, Packet * packet, char * buffer) {
    // get the register massage and process the registration
    // check valid username and password
    char * token = strtok(packet->message, " ");

    // store the username
    char * username = token;

    // check duplicate username
    bool validUserName = true;
    Account * head = accounts;
    while (head != NULL) {
        if (strcmp(token, head->userName) == 0) {
            validUserName = false;
            break;
        }
        head = head->next;
    }

    // store the password
    token = strtok(NULL, " ");
    char * password = token;

    if (!validUserName) {
        memset(packet, 0, sizeof (Packet));
        packet->type = REG_NAK;
        packet->source = 0;
        packet->messageSize = 0;
    }
    else {
        // create a new account struct
        Account * newAccount = malloc(sizeof (Account));
        newAccount->loggedIn = false;
        strcpy(newAccount->userName, username);
        strcpy(newAccount->password, password);
        newAccount->clientId = newAccountIndex++;
        newAccount->next = NULL;

        FILE * newacct = fopen("./accounts.txt", "a+");
        fprintf(newacct, "%s\n", newAccount->userName);
        fprintf(newacct, "%s\n", newAccount->password);
        fclose(newacct);

        // link the new account to the account list
        if (accounts == NULL) {
            accounts = newAccount;
            accountTail = accounts;
        }
        else {
            accountTail->next = newAccount;
            accountTail = accountTail->next;
        }

        memset(packet, 0, sizeof (Packet));
        bzero(packet->message, MESSAGE_LENGTH);
        packet->type = REG_ACK;
        packet->source = 0;
        packet->messageSize = 0;
    }
    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processLogin(int cSokFD, Packet * packet, char * buffer, Account ** acctPtr) {
    // structure of message is "<userName> <password>"
    char * token = strtok(packet->message, " ");
    int clientIdx;

    // find client's index
    Account * head = accounts;
    while (head != NULL) {
        if (strcmp(token, head->userName) == 0) {
            break;
        }
        else {
            head = head->next;
        }
    }

    // authenticate user credential, make sure the userName is valid,
    // password is valid, and this account is not logged in
    token = strtok(NULL, " ");
    if (head == NULL || strcmp(token, head->password) != 0 || head->loggedIn) {
        // if login failed, don't close connection, just tell client login was not successful.
        memset(packet, 0, sizeof (Packet));
        bzero(packet->message, MESSAGE_LENGTH);
        packet->type = LO_NAK;
        packet->source = 0;
        packet->messageSize = 0;
    }
    else {
        // tell client login was successful and its clientIdx
        printf("User with idx %d logged in successfully.\n", head->clientId);
        *acctPtr = head;
        head->sockfd = cSokFD;
        memset(packet, 0, sizeof (Packet));
        packet->type = LO_ACK;
        packet->source = 0;
        sprintf(packet->message, "%d", head->clientId);
        packet->messageSize = strlen(packet->message);
        head->loggedIn = true;
    }
    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processLogout(Packet * packet, Account ** acctPtr) {
    // set account status to not logged in
    (*acctPtr)->loggedIn = false;
    *acctPtr = NULL;
    printf("User %d logged out.\n", packet->source);
}

void processNewSession(int cSokFD, Packet * packet, char * buffer) {
    // if exceeding max session count, tell user session creation failed.
    if (session_count >= MAX_SESSION_COUNT) {
        memset(packet, 0, sizeof (Packet));
        packet->type = NS_NAK;
        packet->source = 0;
        strcpy(packet->message, "Number of clientSession has reached its max.");
        packet->messageSize = strlen(packet->message);
        parsePacketToMessage(packet, buffer);
        send(cSokFD, buffer, MESSAGE_LENGTH, 0);
        return;
    }

    // create new session and connect it to the linked list
    session_count++;
    Session * newSession = malloc(sizeof (Session));
    strcpy(newSession->name, packet->message);
    newSession->clients = NULL;
    newSession->next = NULL;

    Session * head = sessions;
    if (sessions == NULL) {
        sessions = newSession;
    }
    else {
        while (head->next != NULL) {
            head = head->next;
        }
        head->next = newSession;
    }

    // inform client that session was created
    memset(packet, 0, sizeof (Packet));
    packet->type = NS_ACK;
    packet->source = 0;
    packet->messageSize = 0;

    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void listUserOnline(Packet * packet) {
    Account * head = accounts;
    while (head != NULL) {
        if (head->loggedIn) {
            strcat(packet->message, head->userName);
            strcat(packet->message, " ");
        }
        head = head->next;
    }
}

void listSessionInPacket(Packet * packet) {
    // helper function when user queries
    // list all session, if a session is empty, say NO_PARTICIPANT
    // if a session is not empty, list all users in that session
    Session * head = sessions;
    int offset = 0;
    while (head != NULL) {
        strcat(packet->message + offset, head->name);
        offset = strlen(packet->message);
        strcat(packet->message + offset++, " ");
        ClientInfo * ciHead = head->clients;
        if (ciHead == NULL) {
            strcat(packet->message + offset, "NO_PARTICIPANT ");
            offset = strlen(packet->message);
        }
        else {
            while (ciHead != NULL) {
                strcat(packet->message + offset, ciHead->clientName);
                offset = strlen(packet->message);
                strcat(packet->message + offset++, " ");
                ciHead = ciHead->next;
            }
        }
        head = head->next;
    }
}

void processSessionQuery(int cSokFD, Packet * packet, char * buffer) {
    memset(packet, 0, sizeof (Packet));
    packet->type = QS_ACK;
    packet->source = 0;
    listSessionInPacket(packet);
    packet->messageSize = strlen(packet->message);
    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processUserQuery(int cSokFD, Packet * packet, char * buffer) {
    memset(packet, 0, sizeof (Packet));
    packet->type = QU_ACK;
    packet->source = 0;
    listUserOnline(packet);
    packet->messageSize = strlen(packet->message);
    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processJoin(int cSokFD, Packet * packet, char * buffer, Account * userAccount) {
    Session * head = sessions;
    bool found = false;

    // loop through session linked list to find user specified session name
    while (head != NULL) {
        if (strcmp(head->name, packet->message) == 0) {
            found = true;
            break;
        }
        head = head->next;
    }

    // reply based on if server found session matching that name or not
    if (!found) {
        memset(packet, 0, sizeof(Packet));
        packet->type = JN_NAK;
        packet->source = 0;
        strcpy(packet->message, "Unable to find session.");
        packet->messageSize = strlen(packet->message);
    }
    else {
        // create newly joined client's ClientInfo(containing client sockfd)
        // and link it to session's clients list
        ClientInfo * newClient = malloc(sizeof (ClientInfo));
        strcpy(newClient->clientName, userAccount->userName);
        newClient->next = NULL;
        newClient->clientId = packet->source;
        newClient->clientSockfd = cSokFD;

        if (head->clients == NULL) {
            head->clients = newClient;
        }
        else {
            ClientInfo * ciHead = head->clients;
            while (ciHead->next != NULL) {
                ciHead = ciHead->next;
            }
            ciHead->next = newClient;
        }

        // inform client of successful join
        memset(packet, 0, sizeof(Packet));
        packet->type = JN_ACK;
        packet->source = 0;
        strcpy(packet->message, head->name);
        packet->messageSize = strlen(packet->message);
    }
    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processLeave(int cSokFD, Packet * packet, char * buffer) {
    Session * head = sessions;
    bool found = false;

    // locate the session to remove the client from.
    while (head != NULL) {
        if (strcmp(head->name, packet->message) == 0) {
            found = true;
            break;
        }
        head = head->next;
    }

    if (!found) {
        printf("Can't find the session a user is trying to leave.\n");
    }
    else {
        // remove client's ClientInfo from session's clients list
        ClientInfo * front = head->clients;
        if (front == NULL) {
            printf("User list for the session that the client is trying to leave is empty.\n");
        }

        ClientInfo * dummy = malloc(sizeof (ClientInfo));
        dummy->next = head->clients;
        ClientInfo * back = dummy;

        while (front != NULL) {
            if (front->clientId == packet->source) {
                back->next = front->next;
                free(front);
                break;
            }
            front = front->next;
            back = back->next;
        }

        head->clients = dummy->next;
        free(dummy);
    }

    packet->type = LEAVE_SESS_ACK;
    packet->source = 0;
    strcpy(packet->message, head->name);
    packet->messageSize = strlen(packet->message);

    parsePacketToMessage(packet, buffer);
    send(cSokFD, buffer, MESSAGE_LENGTH, 0);
}

void processNewMessage(int cSokFD, Packet * packet, char * buffer) {
    // message from client consists of session that the client is in
    // to help server find session easier
    char packetMessageCopy[MESSAGE_LENGTH] = "";
    strcpy(packetMessageCopy, packet->message);
    char * token = strtok(packet->message, " ");
    Session * head = sessions;
    bool found = false;

    while (head != NULL) {
        if (strcmp(head->name, token) == 0) {
            found = true;
            break;
        }
        head = head->next;
    }

    // parse the actual message from the senderPacket
    int messageStart = strlen(token) + 1;
    token = strtok(NULL, " ");
    char msg[MESSAGE_LENGTH] = "";
    if (token == NULL) {
        printf("Message is empty.\n");
    }
    else {
        strcpy(msg, packetMessageCopy + messageStart);
    }

    if (!found) {
        printf("No session matching message destination.\n");
        return;
    }
    else {
        int sourceClientId = packet->source;

        ClientInfo * ciHead = head->clients;
        if (ciHead == NULL) {
            printf("Client list of session is empty.\n");
        }

        // formulate the senderPacket first, since we'll send the same senderPacket to all client
        memset(packet, 0, sizeof (Packet));
        packet->type = MESSAGE;
        packet->source = 0;
        strcpy(packet->message, head->name);
        strcat(packet->message, ": ");
        strcat(packet->message, msg);
        packet->messageSize = strlen(packet->message);

        // serialize the senderPacket and send it to all clients in the session through ClientInfo's sockfd field
        parsePacketToMessage(packet, buffer);
        while (ciHead != NULL) {
            // skip the original sender of the message
            if (ciHead->clientId != sourceClientId) {
                send(ciHead->clientSockfd, buffer, MESSAGE_LENGTH, 0);
            }
            ciHead = ciHead->next;
        }
    }
}

void processDM(int cSokFD, Packet * packet, char * buffer) {
    // message from the client and will send to a specific user
    // printf("received senderPacket message is : %s\n", senderPacket->message);
    char messageCopy[MESSAGE_LENGTH] = "";
    strcpy(messageCopy, packet->message);
    // token is now the username of sender
    char * token = strtok(packet->message, " ");

    Account * head = accounts;
    Account * target = NULL;
    bool found = false;
    char sender[MESSAGE_LENGTH] = "";
    int targetSockfd = -1;

    while (head != NULL) {
        // get the name of the sender account
        if (head->clientId == packet->source) {
            strcpy(sender, head->userName);
        }
        // cannot send message to yourself
        if (strcmp(token, head->userName) == 0 && head->clientId != packet->source && head->loggedIn) {
            found = true;
            targetSockfd = head->sockfd;
            target = head;
        }
        head = head->next;
    }

    token = strtok(NULL, " ");
    printf("Should be message now %s\n", token);
    char msg[MESSAGE_LENGTH] = "";
    if (strlen(sender) > 0) {
        // concat the message by the format as
        // Message from sender: message
        strcpy(msg, sender);
        strcat(msg, ": ");
        strcat(msg, messageCopy + strlen(target->userName) + 1);
        printf("message to be sent:\n%s\n", msg);
    }

    memset(packet, 0, sizeof (Packet));
    if (!found || strlen(sender) <= 0) {
        packet->type = DM_NAK;
        packet->source = 0;
        strcpy(packet->message, "Unable to find the user or trying to send message to your own account.");
        packet->messageSize = strlen(packet->message);

        parsePacketToMessage(packet, buffer);
        send(cSokFD, buffer, MESSAGE_LENGTH, 0);
    }
    else {
        // telling sender the dm is sent
        memset(packet, 0, sizeof (Packet));
        packet->type = DM_ACK;
        packet->source = 0;
        strcpy(packet->message, msg);
        packet->messageSize = strlen(packet->message);

        parsePacketToMessage(packet, buffer);
        send(cSokFD, buffer, MESSAGE_LENGTH, 0);

        // sending DM to target
        memset(packet, 0, sizeof (Packet));
        packet->type = MESSAGE;
        packet->source = 0;
        strcpy(packet->message, msg);
        packet->messageSize = strlen(packet->message);

        parsePacketToMessage(packet, buffer);
        //printf("Through sockfd %d, Message sent to target: %s\n", targetSockfd, buffer);
        send(targetSockfd, buffer, MESSAGE_LENGTH, 0);
    }
}
