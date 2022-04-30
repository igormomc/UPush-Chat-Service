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

#include "send_packet.c"

#define PORT "3490"
#define MAXDATASIZE 1024



//IPv4, IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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
    struct addrinfo fri, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if(argc < 5){
            printf("Usage: %s <nick> <server_ip> <port> <timeout> <loss_probability>\n", argv[0]);
            return EXIT_SUCCESS;
        }

    float loss_probability = strtol(argv[5], NULL, 10)/100.0f;

    
    memset(&fri, 0, sizeof fri);
    fri.ai_family = AF_UNSPEC;
    fri.ai_socktype = SOCK_DGRAM;
    fri.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &fri, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    for(p = servinfo; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if(p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        return EXIT_FAILURE;
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

    inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&server_addr), s, INET_ADDRSTRLEN);
    //printf("client: connecting to %s\n", s);
    //print welcome to chat nick, server ip, server port
    printf("Welcome to chat %s, server %s, port %s\n", argv[1], argv[2], argv[3]);




    
    while(1){
        printf("Enter message: ");
        fgets(buf, MAXDATASIZE-1, stdin);
        buf[strlen(buf)-1] = '\0';
        if(strcmp(buf, "exit") == 0){
            break;
        }
        //Not allowed to send empty messages, so if the user enters an empty message, the client will not send the message
        //This is normal in the real world, FB, Whatsapp, Snapchat, etc.
        if(strlen(buf) == 0){
            continue;
        }
        send_packet(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    }

        
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';
    printf("client: received '%s'\n",buf);
    close(sockfd);
    return 0;

}

