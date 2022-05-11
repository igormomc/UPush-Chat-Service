#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#include "send_packet.h"

#define PORT "3490"
#define MAXDATASIZE 1400 // max number of bytes we can get at once
// to make more client to be able to connect to the server at once we need to use a queue to store the client socket
void handle_exit(int signal);
static int sockfd = 0;

typedef struct blocked_nickname
{
    char *nick;
    struct client *next;
} blocked_nickname;

struct blocked_nickname *blockHead = NULL;

// Signal handler for SIGALRM
void handle_alarm(int signal)
{
    printf("\nTimeout. ACK not received in time!\n");
    exit(0);
}

// make random  port number bewtween 1024 and 65535 in char
// a tought using this but i found out that putting in 0 will make the port a random port
char *make_random_port()
{
    char *port = malloc(sizeof(char) * 6);
    int random_port = rand() % (65535 - 1024) + 1024;
    sprintf(port, "%d", random_port);
    return port;
}

// randome number between 1 and 100
int random_number()
{
    return rand() % 100 + 1;
}

// get_address function to get the address of the server
void get_address(char *buf, size_t buflen, struct sockaddr_storage addr)
{
    if (addr.ss_family == AF_INET)
    {
        inet_ntop(AF_INET, &(((struct sockaddr_in *)&addr)->sin_addr), buf, buflen); // IPv4
    }
    else
    {
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&addr)->sin6_addr), buf, buflen); // IPv6
    }
}

// get_port is used to get the port number of the client
void get_port(char *buf, size_t buflen, struct sockaddr_storage addr)
{
    if (addr.ss_family == AF_INET)
    {
        sprintf(buf, "%d", ntohs(((struct sockaddr_in *)&addr)->sin_port));
        // add the port number to the struct
    }
    else
    {
        sprintf(buf, "%d", ntohs(((struct sockaddr_in6 *)&addr)->sin6_port));
    }
}

// IPv4, IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// same structure as the one in the server.c
typedef struct client
{
    struct sockaddr_storage *address; // client address will be stored here
    struct timeval timestamp;         // Timestamp of registration
    char *nick;                       // nick name of the client will be stored here and can not be longer than 20 characters
    struct client *next;              // pointer to the next client
} client;

// standard linked list
client *head = NULL;

int isCorrectNickFormat(char *nick)
{
    // we start with setting Correctformat to true
    int correct = 1;
    // can only consist of ASCII characters and spaces, tabs, newlines, and carriage returns
    for (int i = 0; i < (int)strlen(nick); i++)
    {
        if (!((nick[i] >= 'a' && nick[i] <= 'z') || (nick[i] >= 'A' && nick[i] <= 'Z') || (nick[i] >= '0' && nick[i] <= '9')) || (nick[i] == ' ' || nick[i] == '\t' || nick[i] == '\n' || nick[i] == '\r'))
        {
            correct = 0;
        }
    }
    // cannot be longer than 20 bytes
    if (strlen(nick) < 1 || strlen(nick) > 20)
    {
        correct = 0;
    }

    return correct;
}

// very standard way to add a new client to the linked list
void add_client_to_list(char *nick, struct sockaddr_storage *addr_storage)
{
    client *new_client = malloc(sizeof(client));
    new_client->address = malloc(sizeof(struct sockaddr_storage));
    new_client->nick = malloc((strlen(nick) + 1) * sizeof(char));
    strcpy(new_client->nick, nick);
    new_client->next = NULL;
    memset(new_client->address, 0, sizeof(struct sockaddr_storage));
    memcpy(new_client->address, addr_storage, sizeof(struct sockaddr_storage));

    if (head == NULL)
    {
        head = new_client;
    }
    else
    {
        client *current = head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = new_client;
    }
}

// add nick to linkedlist of blocked nicknames
void add_blocked_nickname(char *nick)
{
    remove_blocked_nickname(nick);
    blocked_nickname *blockNick = malloc(sizeof(blocked_nickname));
    blockNick->nick = malloc((strlen(nick) + 1) * sizeof(char));
    strcpy(blockNick->nick, nick);
    blockNick->next = NULL;

    if (blockHead == NULL)
    {
        blockHead = blockNick;
    }
    else
    {
        blocked_nickname *current = blockHead;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = blockNick;
    }
    // remove if nick is already in the list
    // print out the blocked nicknames
    blocked_nickname *current = blockHead;
    while (current != NULL)
    {
        printf("%s\n", current->nick);
        current = current->next;
    }
}

void remove_blocked_nickname(char *nick)
{
    blocked_nickname *current = blockHead;
    blocked_nickname *prev = NULL;
    while (current != NULL)
    {
        if (strcmp(current->nick, nick) == 0)
        {
            if (prev == NULL)
            {
                blockHead = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            free(current->nick);
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
}

// is_blocked function to check if the nick is blocked
int is_blocked(char *nick)
{
    blocked_nickname *current = blockHead;
    while (current != NULL)
    {
        if (strcmp(current->nick, nick) == 0)
        {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// very standard way to check if a client is in the linked list already or not (used for the lookup)
int isClientInList(char *nick)
{
    struct client *current = head;
    while (current != NULL)
    {
        if (strcmp(current->nick, nick) == 0)
        {
            return 1;
        }
        else
        {
            current = current->next;
        }
    }
    return 0;
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, handle_exit);
    char nick[20];
    int numbytes;
    char buf[MAXDATASIZE];
    char send_message[MAXDATASIZE];
    struct addrinfo fri, *servinfo, *p, *clientinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char *nickToMess;

    memset(buf, 0, MAXDATASIZE);

    if (argc != 6)
    {
        printf("Usage: %s <nick> <server_ip> <port> <timeout> <loss_probability>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    // declere all variables from the command line
    strcpy(nick, argv[1]);
    const char *port = argv[3];
    int timeout = atoi(argv[4]);

    float loss_probability = strtol(argv[5], NULL, 10) / 100.0f;
    // if the loss_probability is not between 0 and 1, print error message and exit
    if (loss_probability < 0 || loss_probability > 1)
    {
        printf("Loss probability must be between 0 and 1\n");
        return EXIT_SUCCESS;
    }

    // if nick is not between 1 and 20 and have space, tab, or return character, print error message and exit
    if (strlen(nick) < 1 || strlen(nick) > 20 || strchr(nick, ' ') != NULL || strchr(nick, '\t') != NULL || strchr(nick, '\n') != NULL)
    {
        printf("Nick must be between 1 and 20 characters and can not contain space, tab, or return\n");
        return EXIT_SUCCESS;
    }

    // if nick containts not ascii character, print error message and exit
    int l = strlen(nick);
    for (int i = 0; i < l; i++)
    {
        if (!(nick[i] >= 'a' && nick[i] <= 'z') && !(nick[i] >= 'A' && nick[i] <= 'Z') && !(nick[i] >= '0' && nick[i] <= '9'))
        {
            printf("Nick can only contain ascii characters\n");
            return EXIT_SUCCESS;
        }
    }

    memset(&fri, 0, sizeof fri);
    fri.ai_family = AF_INET6; // this handles both ipv4 and ipv6 addresses
    fri.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &fri, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);

    // if we didn't get a socket, print error message and exit
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return EXIT_FAILURE;
    }

    // if port is not between 1024 and 65535, print error message and exit
    if (strtol(port, NULL, 10) < 1024 || strtol(port, NULL, 10) > 65535)
    {
        printf("Port must be between 1024 and 65535\n");
        return EXIT_SUCCESS;
    }

    // if timeout is not between 1 and 60, print error message and exit
    //if (timeout < 1 || timeout > 60)
    //{   printf("Timeout must be between 1 and 60\n");
    //    return EXIT_SUCCESS;
    //}

    srand48(time(0)); // have to call strand48 before using drand48, otherwise it will always return wrong, read it in mane pages
    // setting loss_probability
    set_loss_probability(loss_probability);

    struct sockaddr_storage server_addr;

    // SOCK_DGRAM is used for UDP sockets
    memset(&fri, 0, sizeof fri);
    fri.ai_family = AF_UNSPEC;
    fri.ai_socktype = SOCK_DGRAM;

    // get the address info of the server
    if ((rv = getaddrinfo(argv[2], argv[3], &fri, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    // here we are getting the address info of the server and storing it in the server_addr struct so we can use it later
    memset(&server_addr, 0, sizeof(struct sockaddr_storage));
    memcpy(&server_addr, servinfo->ai_addr, servinfo->ai_addrlen);
    freeaddrinfo(servinfo);                                                                 // copy the address info of the server to the server_addr
    inet_ntop(AF_INET6, get_in_addr((struct sockaddr *)&server_addr), s, INET6_ADDRSTRLEN); // get the ip address of the server

    printf("Welcome to chat %s, server %s, port %s\n", argv[1], argv[2], argv[3]);
    // print cleints port

    // get random number that changes every time the client send a REGISTER message
    srand(time(0)); // will not give random number if not calling on srand()
    int number = rand() % 100;

    // making reg message and send it to the server
    sprintf(send_message, "PKT %d REG %s", number, argv[1]);
    send_packet(sockfd, send_message, strlen(send_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    signal(SIGALRM, handle_alarm); // set the timeout handler
    alarm(timeout); // set the timeout
    // Receive the message from the server
    if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE, 0, NULL, NULL)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }
    alarm(0); // cancel the timeout
    signal(SIGALRM, SIG_DFL); // cancel the timeout handler

    // free(send_message);
    //  set timeout for recvfrom
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

    /**
     while(1){
         sleep(10);
         char *send_message = malloc(sizeof(char) * (strlen(argv[1]) + 10));
         //get random number that changes every time the client send a REGISTER message
         srand(time(0));
         int number = rand() % 100;

         sprintf(send_message, "PKT %d REG %s", number, argv[1]);
         send_packet(sockfd, send_message, strlen(send_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
         // Receive the message from the server
         if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE, 0, NULL, NULL)) == -1)
         {
             perror("recvfrom");
             exit(1);
         }
         free(send_message);
         // set timeout for recvfrom
         struct timeval tv;
         tv.tv_sec = timeout;
         tv.tv_usec = 0;
         setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
     }
     **/

    // fd_set for select
    fd_set readfds;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    printf("Registation Succsessfull, %s\n", nick);
    int selectReturn;
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        // Send heartbeat to server every 10 seconds
        struct timeval heartbeat;
        heartbeat.tv_sec = 10;
        heartbeat.tv_usec = 0;

        selectReturn = select(FD_SETSIZE, &readfds, NULL, NULL, &heartbeat);

        if (selectReturn == 0)
        {
            // get random number that changes every time the client send a REGISTER message

            srand(time(0)); // will not give random number if not calling on srand()
            int number = rand() % 100;

            // making reg message and send it to the server
            sprintf(send_message, "PKT %d REG %s", number, argv[1]);
            send_packet(sockfd, send_message, strlen(send_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
            // Receive the message from the server

            if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE, 0, NULL, NULL)) == -1)
            {
                perror("recvfrom");
                exit(1);
            }
            // set timeout for recvfrom
            struct timeval tv;
            tv.tv_sec = timeout;
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
        }
        int rc;
        if (FD_ISSET(sockfd, &readfds))
        {
            rc = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
            buf[rc] = '\0';

                printf("BLABLABLA: %s\n", buf);
            // Correct Format: "PKT nummer FROM fra_nick TO til_nick MSG tekst"
            char *wordForWord[8];
            int i = 0;
            char *word = strtok(buf, " ");
            while (word != NULL)
            {
                if (i == 6)
                {
                    wordForWord[i] = word;
                    word = strtok(NULL, "\0");
                }
                else
                {
                    wordForWord[i] = word;
                    word = strtok(NULL, " ");
                }
                i++;
            }

            // check if the format is correct: PKT nummer FROM fra_nick TO til_nick MSG tekst
            if (strcmp(wordForWord[0], "PKT") == 0 && wordForWord[1] != NULL && strcmp(wordForWord[2], "FROM") == 0 && wordForWord[3] != NULL && strcmp(wordForWord[4], "TO") == 0 && wordForWord[5] != NULL && strcmp(wordForWord[6], "MSG") == 0)
            {
                // check if to_nick is the same as the client's nick
                if (strcmp(wordForWord[5], nick) == 0)
                {
                    sprintf(send_message, "PKT %d ACK %s", atoi(wordForWord[1]), wordForWord[3]);
                    send_packet(sockfd, send_message, strlen(send_message), 0, (struct sockaddr *)&client_addr, client_addr_len);
                    printf("%s: %s\n", wordForWord[3], wordForWord[7]);
                }
                else
                {
                    printf("Wrong Format!");
                }
            }
        }
        // check if there is messege from STDIN_FILENO
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            // memset the buffer to 0 to avoid garbage data
            memset(buf, 0, MAXDATASIZE);
            nickToMess = strtok(buf, " ");

            fgets(buf, MAXDATASIZE - 1, stdin);
            buf[strlen(buf) - 1] = '\0';
            // if the message is QUIT, exit everyting
            if (strcmp(buf, "QUIT") == 0)
            {

                if (head != NULL)
                {
                    client *remove = head;
                    while (remove != NULL)
                    {
                        client *tRem = remove;
                        remove = remove->next;
                        free(tRem->nick);
                        free(tRem->address);
                        free(tRem);
                    }
                }

                if (blockHead != NULL)
                {
                    blocked_nickname *r = blockHead;
                    while (r != NULL)
                    {
                        blocked_nickname *rem = r;
                        r = r->next;
                        free(rem->nick);
                        free(rem);
                    }
                }

                // loop through the blockedList and free the memory

                printf("Exiting The server, Good bye!\n");
                // Maybe free more memory here???
                close(sockfd);
                free(nickToMess);
                break;
            }

            if (strlen(buf) == 0)
            {
                printf("Not allowed with empty message\n");
                free(nickToMess);
                continue;
            }
            // if buf length is too long print error
            if (strlen(buf) > MAXDATASIZE - 1)
            {
                printf("To long message\n");
                buf[MAXDATASIZE - 1] = '\0';
                continue;
            }

            // block message into words on space
            int blockFormat = 0;
            // Getting the message and the nick to send it to
            char *toNick; // the nick that the message is going to
            toNick = strtok(buf, " ");
            char *theMessageCopy = strtok(NULL, "\0");
            char *theMessage = NULL; // the message
            if (theMessageCopy != NULL)
            {
                theMessage = strdup(theMessageCopy);
            }
            //  hack to fix a bug where a empty message is sent
            else
            {
                theMessage = strdup("");
            }
            int correctMessageFormat = 0;

            if (strcmp(toNick, "BLOCK") == 0)
            {
                char *nick1;
                char *word = strtok(theMessage, " ");

                // if word is null or empty string or space print error
                if (word == NULL || strlen(word) == 0 || strcmp(word, " ") == 0)
                {
                    printf("Who do you want to Block?\n");
                    free(theMessage);
                    continue;
                }

                nick1 = strdup(word);
                if (strtok(NULL, " ") != NULL)
                {
                    printf("wrong format\n");
                    continue;
                }

                else
                {
                    blockFormat = 1;
                    add_blocked_nickname(nick1);
                    free(nick1);
                }
            }

            if (strcmp(toNick, "UNBLOCK") == 0)
            {
                char *nick1;
                char *word = strtok(theMessage, " ");

                // if word is null or empty string or space print error
                if (word == NULL || strlen(word) == 0 || strcmp(word, " ") == 0)
                {
                    printf("Who do you want to UnBlock?\n");
                    free(theMessage);
                    continue;
                }

                nick1 = strdup(word);
                if (strtok(NULL, " ") != NULL)
                {
                    printf("wrong format\n");
                    continue;
                }

                else
                {
                    blockFormat = 1;
                    remove_blocked_nickname(nick1);
                    free(nick1);
                }
            }

            // check if the message is correct format
            int l2 = strlen(toNick);
            if (toNick != NULL && toNick[0] == '@' && strlen(toNick) > 1)
            {
                // chekc that nick dose not contain ascill characters
                for (int i = 1; i < l2; i++)
                {
                    // if the character is not ascill, we set correctMessageFormat to 1
                    if (isalpha(toNick[i]) == 0)
                    {
                        printf("Nickname can not contain special characters\n");
                        correctMessageFormat = 1;
                        break;
                    }
                }
                if (theMessage != NULL)
                {
                    correctMessageFormat = 1;
                    nickToMess = strdup(toNick + 1);
                }
            }

            // if length of themessage is 0, set format to 0 (bug fix)
            if (strlen(theMessage) == 0)
            {
                correctMessageFormat = 0;
            }
            // if nick contains num
            if (correctMessageFormat == 1)
            {
                // function from above to check nick if its allowed to send message
                if (isCorrectNickFormat(nickToMess) == 1 && blockFormat == 0)
                {
                    // function from above to check if client is in the linklist, if not we cant send message to the Nick
                    if (isClientInList(nickToMess) == 0)
                    {
                        // check if nick contains special characters
                        srand(time(0)); // will not give random number if not calling on srand()
                        int number = rand() % 100;
                        sprintf(send_message, "PKT %d LOOKUP %s", number, nickToMess);

                        // TODO: POSSIBLE BUG MAYBE CHANGE sizeof(server_addr)); IPv6 ELLER IPv4
                        send_packet(sockfd, send_message, strlen(send_message) + 1, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                        memset(buf, 0, MAXDATASIZE);
                        int numbytes = recvfrom(sockfd, buf, MAXDATASIZE - 1, 0, NULL, NULL);
                        buf[numbytes] = '\0';
                        printf("%s\n", buf);
                        // If response has "NOT FOUND" in it, then the client isn't registered on the server
                        if (strstr(buf, "NOT FOUND") != NULL)
                        {
                            printf("%s is not in server\n", nickToMess);
                            free(nickToMess);
                            free(theMessage);
                        }
                        else
                        {
                            // splitting the message to get the variables
                            char *wordForWord[7];
                            int i = 0;
                            char *word = strtok(buf, " ");
                            while (word != NULL)
                            {
                                wordForWord[i] = word;
                                word = strtok(NULL, " ");
                                i++;
                            }

                            // getting variables from the message
                            char *nickname = wordForWord[3];
                            char *ip = wordForWord[4];
                            char *port = wordForWord[6];

                            struct sockaddr_storage clientAddress;

                            // works for IPv4 and IPv6 addresses
                            memset(&fri, 0, sizeof fri);
                            fri.ai_family = AF_UNSPEC;
                            fri.ai_socktype = SOCK_DGRAM;

                            if ((rv = getaddrinfo(ip, port, &fri, &clientinfo)) != 0)
                            {
                                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                                return EXIT_FAILURE;
                            }

                            memset(&clientAddress, 0, sizeof(struct sockaddr_storage));
                            memcpy(&clientAddress, clientinfo->ai_addr, clientinfo->ai_addrlen);
                            clientAddress.ss_family = clientinfo->ai_family;

                            // free the clientinfo
                            freeaddrinfo(clientinfo);

                            char adderBuffer[INET6_ADDRSTRLEN];
                            get_address(adderBuffer, INET6_ADDRSTRLEN, clientAddress);

                            add_client_to_list(nickname, &clientAddress);
                            srand(time(0)); // will not give random number if not calling on srand()
                            int number = rand() % 100;
                            sprintf(send_message, "PKT %d FROM %s TO %s MSG %s", number, nick, nickname, theMessage);
                            // if nick is in blocked_nickname list, we dont send the message
                            if (is_blocked(nickname) == 0)
                            {
                                send_packet(sockfd, send_message, strlen(send_message) + 1, 0, (struct sockaddr *)&clientAddress, sizeof(clientAddress));
                                free(nickToMess);
                                free(theMessage);
                            }
                            else
                            {
                                printf("%s is blocked\n", nickname);
                                free(nickToMess);
                                free(theMessage);
                            }
                        }
                    }
                    else
                    {
                        struct client *currentClient = head;
                        while (currentClient != NULL)
                        {
                            if (strcmp(nickToMess, currentClient->nick) == 0)
                            {
                                // gathering the address of the client that we want to send the message to
                                struct sockaddr_storage clientSend;
                                char adderBuffer[INET6_ADDRSTRLEN];
                                get_address(adderBuffer, INET6_ADDRSTRLEN, *currentClient->address);
                                char portBuffer[INET6_ADDRSTRLEN];
                                get_port(portBuffer, INET6_ADDRSTRLEN, *currentClient->address);
                                // making message and sending it to the client
                                srand(time(0)); // will not give random number if not calling on srand()
                                int number = rand() % 100;
                                sprintf(send_message, "PKT %d FROM %s TO %s MSG %s", number, nick, nickToMess, theMessage);
                                if (is_blocked(nickToMess) == 0)
                                {
                                    send_packet(sockfd, send_message, strlen(send_message) + 1, 0, (struct sockaddr *)currentClient->address, sizeof(*currentClient->address));
                                    free(nickToMess);
                                    free(theMessage);
                                    break;
                                }
                                else
                                {
                                    printf("%s is blocked\n", nickToMess);
                                    free(nickToMess);
                                    free(theMessage);
                                    break;

                                }
                            }
                            currentClient = currentClient->next;
                        }
                    }
                }
                else
                {

                    free(theMessage);
                    free(nickToMess);
                    printf("Nickname is not correct format or maybe wrong nick\n");
                    exit(1);
                }
            }
            else
            {
                if (blockFormat = 0)
                {
                    free(theMessage);
                    free(nickToMess);
                    printf("Wrong..!\n");
                }
                else
                {
                    free(theMessage);
                    free(nickToMess);
                }
            }
        }
    }
}

#pragma gcc diagnostic push
#pragma gcc diagnostic ignored "-Wunused-parameter"
void handle_exit(int signal)
{
#pragma gcc diagnostic pop
    if (sockfd != 0)
        close(sockfd);
    // when cloing we have to remove the clients from the list and free the memory.
    if (head != NULL)
    {
        client *remove = head;
        while (remove != NULL)
        {
            client *tRem = remove;
            remove = remove->next;
            free(tRem->nick);
            free(tRem->address);
            free(tRem);
        }
    }
    if (blockHead != NULL)
    {
        blocked_nickname *r = blockHead;
        while (r != NULL)
        {
            blocked_nickname *rem = r;
            r = r->next;
            free(rem->nick);
            free(rem);
        }
    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}