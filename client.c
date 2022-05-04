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

#include "send_packet.c"

#define PORT "3490" 
#define MAXDATASIZE 1400 // max number of bytes we can get at once
//to make more client to be able to connect to the server at once we need to use a queue to store the client socket 

//make random  port number bewtween 1024 and 65535 in char
char* make_random_port()
{
    char* port = malloc(sizeof(char)*6);
    int random_port = rand() % (65535 - 1024) + 1024;
    sprintf(port, "%d", random_port);
    return port;
}

//IPv4, IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//same structure as the one in the server.c
typedef struct client {
        struct sockaddr_storage address; // client address will be stored here TODO: Use sockaddr_storage for ipv4 ipv6 comp.
        struct timeval timestamp; // Timestamp of registration (check below 30 sec before giving to lookup)
        char *nick; // nick name of the client will be stored here and can not be longer than 20 characters
        struct client *next; // pointer to the next client
    } client;

client *head = NULL;



//REWRITE
int validateNickname(char *nick){
    int validNickname = 1;

    //Check if nickname is only ascii characters 
    for(int i = 0; i < (int)strlen(nick); i++){
        if(!((nick[i] >= 'a' && nick[i] <= 'z') || (nick[i] >= 'A' && nick[i] <= 'Z') || (nick[i] >= '0' && nick[i] <= '9'))){
        validNickname = 0;
        break;
        }
    }

    //Check if nickname is between 1 and 20 characters long 
    if(strlen(nick) < 1 || strlen(nick) > 20){
        validNickname = 0;
    }

    //Check that nickname does not containt white space, tab or return 
    for(int i = 0; i < (int)strlen(nick); i++){
        if(nick[i] == ' ' || nick[i] == '\t' || nick[i] == '\n'){
            validNickname = 0;
            break;
        }
    }

    return validNickname;
}

//REWRITE
void add_client(char *nick, struct sockaddr_storage addr_storage){
    client *new_client = malloc(sizeof(client));
    new_client->address = addr_storage;
    new_client->nick = malloc((strlen(nick) + 1) * sizeof(char));
    strcpy(new_client->nick, nick);
    new_client->next = NULL;

    //If the list is empty, make the new client the head
    if(head == NULL){
        head = new_client;
    }
    else{
        //Iterate through the list and add the new client to the end
        client *current = head;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = new_client;
    }
}

//REWRITE
int checkClientExistence(char *nick){
    struct client *current = head;
    while(current != NULL){
        if(strcmp(current->nick, nick) == 0){
            return 1;
        }
        current = current->next;
    }
    return 0;
}


int main(int argc, char const *argv[])
{
//When a client sends a lookup message, it contains a nick. The format of the lookup message is “PKT number LOOKUP nick”. The server checks if this nick is registered. If it is, the server returns the nick,
//its IP address and port to the client that sent the lookup message. A successful lookup should result in a message on the form “ACK number NICK nick IP address PORT port”. If it is not registered, the server returns a not-found message, which should be on the form “ACK number NOT FOUND”.
//A UPush client is a program that takes the user's nick, the IP address and port of a UPush server, a timeout value in seconds and a loss probability on the command line. When the UPush client starts, it tries to register its nick with the server immediately (i.e. by sending “PKT number REG nick”). It waits for either an OK message from the server or a timeout. If the timeout comes first, the UPush client prints an error message and exits. If the client receives an OK message, it enters an event loop to wait for both UDP packets and user input, at the same time. This event loop is called the main event loop.

//Read parameters from the command line
    char nick[20]; 
    //struct hostent *server_host;

    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    char mess[MAXDATASIZE];
    struct addrinfo fri, *servinfo, *p, *clientinfo;
    int rv;
    char s[INET6_ADDRSTRLEN];
    //get clients adress length
    socklen_t addr_len;



    if(argc != 6){
            printf("Usage: %s <nick> <server_ip> <port> <timeout> <loss_probability>\n", argv[0]);
            return EXIT_SUCCESS;
        }

    //declere all variables from the command line
    strcpy(nick, argv[1]);
    char* port = argv[3];
    int timeout = atoi(argv[4]);
    
    
    float loss_probability = strtol(argv[5], NULL, 10)/100.0f;
    //if the loss_probability is not between 0 and 1, print error message and exit
    if(loss_probability < 0 || loss_probability > 1){
        printf("Loss probability must be between 0 and 1\n");
        return EXIT_SUCCESS;
    }


    //if nick is not between 1 and 20 and have space, tab, or return character, print error message and exit
    if(strlen(nick) < 1 || strlen(nick) > 20 || strchr(nick, ' ') != NULL || strchr(nick, '\t') != NULL || strchr(nick, '\n') != NULL){
        printf("Nick must be between 1 and 20 characters and can not contain space, tab, or return\n");
        return EXIT_SUCCESS;
    }

    //if nick containts not ascii character, print error message and exit
    for(int i = 0; i < strlen(nick); i++){
        if(!(nick[i] >= 'a' && nick[i] <= 'z') && !(nick[i] >= 'A' && nick[i] <= 'Z') && !(nick[i] >= '0' && nick[i] <= '9')){
            printf("Nick can only contain ascii characters\n");
            return EXIT_SUCCESS;
        }
    }
    
    memset(&fri, 0, sizeof fri);
    fri.ai_family = AF_INET6;
    fri.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, "0", &fri, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        break;
    }


    if(p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        return EXIT_FAILURE;
    }

    //if port is not between 1024 and 65535, print error message and exit
    if(strtol(port, NULL, 10) < 1024 || strtol(port, NULL, 10) > 65535){
        printf("Port must be between 1024 and 65535\n");
        return EXIT_SUCCESS;
    }


    freeaddrinfo(servinfo);
    srand48(time(0)); //have to call strand48 before using drand48 
    set_loss_probability(loss_probability);

    struct sockaddr_storage server_addr;

    memset(&fri, 0, sizeof fri);
    fri.ai_family = AF_UNSPEC;
    fri.ai_socktype = SOCK_DGRAM;


    if((rv = getaddrinfo(argv[2], argv[3], &fri, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_storage)); 
    memcpy(&server_addr, servinfo->ai_addr, servinfo->ai_addrlen); 
    inet_ntop(AF_INET6, get_in_addr((struct sockaddr *)&server_addr), s, INET6_ADDRSTRLEN); 
    

    //printf("client: connecting to %s\n", s);
    //print welcome to chat nick, server ip, server port
    printf("Welcome to chat %s, server %s, port %s\n", argv[1], argv[2], argv[3]);
    //print cleints port


    char *send_message = malloc(sizeof(char)*(strlen(argv[1])+10));
    char number = 10;

    sprintf (send_message, "PKT %d REG %s", number, argv[1]);
    send_packet(sockfd, send_message, strlen(send_message),0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    free(send_message);

    //set timeout for recvfrom
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    


    //print out messege received from server
    //printf("%s\n", buf);
    //send register message to server every 10 seconds
    /**
    while(1){
        sleep(10);
        send_packet(sockfd, "PKT 10 REG", strlen("PKT 10 REG"),0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }
    **/

    //fd_set for select
    fd_set readfds; 
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    printf("Registation Succsessfull, %s\n", nick);
    while(1){
        //print register message
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        //select
        if(select(sockfd+1, &readfds, NULL, NULL, NULL) == -1){
            perror("timeout...");
            exit(EXIT_FAILURE);
        }
        int rc;
        if(FD_ISSET(sockfd, &readfds)) {
            rc = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
            buf[rc] = '\0';
            printf("%s", buf);
        }


        //check if there is messege from STDIN_FILENO
        if(FD_ISSET(STDIN_FILENO, &readfds)){
            //send message to server
            //reaf the message from STDIN_FILENO
            fgets(buf, MAXDATASIZE-1, stdin);
            buf[strlen(buf)-1] = '\0';
            //if the message is exit, exit
            if(strcmp(buf, "exit") == 0){
                printf("Exiting The server, Good bye!\n");
                //Maybe free the memory here???
                close(sockfd);
                break;
            }

            if(strlen(buf) == 0){
                continue;
            }

            if(strlen(buf) > MAXDATASIZE-1){
                printf("To long message\n");
                buf[MAXDATASIZE-1] = '\0';
                continue;
            }

            //Get the first word of the buffer
            char *toNick = strtok(buf, " ");
            //Save the rest of the buffer in a variable 
            char *theMessage = strtok(NULL, "\0");

            //Variable boolean for correct message format 
            int correctMessageFormat = 0;
            //Variable to hold nickname from message 
            char *nicknameFromMessage = NULL;

            //Check that the first word isnt NULL and that the first word starts with @ and has at least one character after @ 
            if(toNick != NULL && toNick[0] == '@' && strlen(toNick) > 1){
                //Check if the rest of the buffer is not empty 
                if(theMessage != NULL){
                    //Set correctMessageFormat to 1
                    correctMessageFormat = 1;

                    //Get the nickname from the message
                    nicknameFromMessage = strtok(toNick, "@");
                }
            }

            if(correctMessageFormat == 1){
                //Validate the nickname from the message
                if(validateNickname(nicknameFromMessage) == 1){
                    
                    if(checkClientExistence(nicknameFromMessage) == 0){
                        //print nick is not in server
                        printf("%s is not in nick cache! Need to lookup.\n", nicknameFromMessage);
                        //Create a LOOKUP message to send to the server with the format PKT packet_number LOOKUP nickname 
                        char *send_message = malloc(sizeof(char)*(strlen(nicknameFromMessage)+10));
                        sprintf(send_message, "PKT 0 LOOKUP %s", nicknameFromMessage);
                        //TODO: POSSIBLE BUG MAYBE CHANGE sizeof(server_addr)); IPv6 ELLER IPv4
                        int sent_bytes = send_packet(sockfd, send_message, strlen(send_message)+1,0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                        fprintf(stderr, "Sent: %d\n", sent_bytes);
                        perror("send_packet");
                        //Receive message from server, server_addres
                        int numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1, 0, NULL, NULL);
                        //Print the message from the server
                        buf[numbytes] = '\0';
                        printf("Numbytes: %d\n", numbytes);
                        perror("recvfrom");
                        printf("%s\n", buf);
                        //If response has "NOT FOUND" in it, then the client isn't registered on the server 
                        if(strstr(buf, "NOT FOUND") != NULL){
                            printf("%s is not in server\n", nicknameFromMessage);
                        }
                         else{
                                //Print out that we are here
                                //The response has this format ACK packet_number NICK nickname IP ip_address PORT port_number
                                //Split the buffer into an array of strings using strtok using space as the delimiter 
                                char *splitBuffer[7];
                                int i = 0;
                                char *token = strtok(buf, " ");
                                while(token != NULL){
                                    splitBuffer[i] = token;
                                    token = strtok(NULL, " ");
                                    i++;
                                }

                                //Get nickname, Ip-address and Port from the splitBuffer array “ACK number NICK nick IP address PORT port”
                                char *nickname = splitBuffer[2];
                                char *ip = splitBuffer[4];
                                char *port = splitBuffer[6];
                                printf("%s at %s:%s\n", nickname, ip, port);


                                //Create a struct sockaddr_storage variable to hold the client's address and add ip-address and port to it 
                                struct sockaddr_storage clientAddress;

                               
                                inet_pton(AF_INET6, ip, &(((struct sockaddr_in6*)&clientAddress)->sin6_addr));
                                memset(&fri, 0, sizeof fri);
                                fri.ai_family = AF_UNSPEC;
                                fri.ai_socktype = SOCK_DGRAM;


                                if((rv = getaddrinfo(argv[2], argv[3], &fri, &clientinfo)) != 0){
                                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                                    return EXIT_FAILURE;
                                }

                                memset(&clientAddress, 0, sizeof(struct sockaddr_storage)); 
                                memcpy(&clientAddress, clientinfo->ai_addr, clientinfo->ai_addrlen); 

                                //something more ?                                

                                //Add newClient to nick-cache linked list using addClient(nickname, struct sockaddr_storage *address)
                                add_client(nickname, clientAddress);

                                //Find the client in the linked-list using a loop
                                struct client *currentClient = head;
                                while(currentClient != NULL){
                                    //Check if the nickname from the message is equal to the nickname from the linked-list
                                    if(strcmp(nickname, currentClient->nick) == 0){
                                        //Print out that the client is in the linked-list
                                        printf("Client %s is in the linked-list\n", nicknameFromMessage);
                                        break;
                                    }
                                    //Increment currentClient to the next client in the linked-list
                                    currentClient = currentClient->next;
                                }
                            }
                    }
                } else {
                    //If the message is not in the correct format, print out an error message
                    printf("Incorrect message format\n");
                }
            }
        }
    }
}


//Find client in linked list by nickname, ip and port
   