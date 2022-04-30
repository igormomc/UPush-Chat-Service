#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "send_packet.c"
#define BUF_SIZE 1460


void check_error(int result, char* message)
{
    if (result < 0)
    {
        perror(message);
        exit(EXIT_FAILURE);
    }
}

typedef struct client {
        struct sockaddr_storage address; // client address will be stored here TODO: Use sockaddr_storage for ipv4 ipv6 comp.
        struct timeval timestamp; // Timestamp of registration (check below 30 sec before giving to lookup)
        char nick[20]; // nick name of the client will be stored here and can not be longer than 20 characters
        struct client *next; // pointer to the next client
    } client;
client *head = NULL;
int main(int argc, char const *argv[]){
    int rc;
    int sockfd;
    struct addrinfo fri, *servinfo, *p;
    fd_set fds;
    struct timeval timeout;
    char buf[BUF_SIZE];
    
    if(argc < 3){
        printf("Usage: %s <port> <loss_probability>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    float loss_probability = strtol(argv[2], NULL, 10)/100.0f;

    // Bind socket to port
    memset (&fri, 0, sizeof(fri));
    fri.ai_family = AF_UNSPEC; // use IPv4 or IPv6
    fri.ai_socktype = SOCK_DGRAM; // UDP
    fri.ai_flags = AI_PASSIVE; // This is for binding to all interfaces
    
    if((rc = getaddrinfo(NULL, argv[1], &fri, &servinfo)) != 0){ //argv[1] is the port
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("socket");
            continue;
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("bind");
            continue;
        }
        break;
    }

    if(p == NULL){
        fprintf(stderr, "Failed to bind socket\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Server started on port %s\n", argv[1]);
    }

    //Free the memory allocated by getaddrinfo
    freeaddrinfo(servinfo);

    // set loss probability for the send_packet function
    srand48(time(0)); //have to call strand48 before using drand48 
    set_loss_probability(loss_probability);
    //printf("Loss probability is %f\n", loss_probability);

    printf("Welcome To the IN2140 Chat! Enjoi:)\n");
    
    buf[0] = 0;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    //THis loop will run until the server is terminated, it will listen for incoming
    while (1){
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        FD_SET(STDIN_FILENO, &fds);
        timeout.tv_sec = 1;
        rc = select(FD_SETSIZE, &fds, NULL, NULL, 0);
        check_error(rc, "select");

        struct client *current = head;
            while(current != NULL){
                //if the client is not registered for more than 30 seconds, remove it from the list
                if(difftime(time(NULL), current->timestamp.tv_sec) > 30){
                    printf("%s has been removed, Longer then 30 sec since Registration\n", current->nick); 
                    struct client *temp = current;
                    current = current->next;
                    if(temp == head){
                        head = current;
                    }
                    else{
                        struct client *previous = head;
                        while(previous->next != temp){
                            previous = previous->next;
                        }
                        previous->next = current;
                    }
                    //free the memory allocated for the temp client, because it is not needed anymore
                    free(temp);
                }
                else{
                    current = current->next;
                }
            }

        if (FD_ISSET(sockfd, &fds)){
            rc = recvfrom(sockfd, buf, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
            buf[rc] = '\0';
            check_error(rc, "recvfrom");
            printf("%s\n", buf);

            char *splitat = " ";
            char *split = strtok(buf, splitat);
            if(strcmp(split, "PKT")==0){
                // nummer
                split = strtok(NULL, splitat);
                if (split == NULL) continue;
                char pkt[256];
                strcpy(pkt, split);

                split = strtok(NULL, splitat);
                if (split == NULL) continue;
                if(strcmp(split, "REG")==0){
                    // nick
                    split = strtok(NULL, splitat);
                    char *nick = malloc(strlen(split)+1);
                    strcpy(nick, split); 

                    //check if nick is in Linked List
                    struct client *currentcheck = head;
                    while(currentcheck != NULL){
                        if(strcmp(currentcheck->nick, nick) == 0){
                            //remove nick from list 
                            printf("%sremoved\n", nick);
                            struct client *temp = currentcheck;
                            currentcheck = currentcheck->next;
                            if(temp == head){
                                head = currentcheck;
                            }
                            else{
                                struct client *previous = head;
                                while(previous->next != temp){
                                    previous = previous->next;
                                }
                                previous->next = currentcheck;
                            }
                            free(temp);
                        }
                        else{
                            currentcheck = currentcheck->next;
                        }
                    }
                    
                    client *new_client = malloc(sizeof(client));
                    new_client->next = head;
                    head = new_client;
                    new_client->address = client_addr;
                    gettimeofday(&new_client->timestamp, NULL); //dont know if this is needed
                    strcpy(new_client->nick, nick);
                    printf("Client %s registered\n", nick);
                    
                    //show all  clients
                    /**client *current = head;
                    while(current != NULL){
                        printf("%s\n", current->nick);
                        current = current->next;
                    }**/
                    
                    
                    //make char variable for nick + packet
                    char ack[8 + strlen(pkt)];
                    strcpy(ack, "ACK ");
                    strcat(ack, pkt);
                    strcat(ack, " OK");
                    
                    send_packet(sockfd, ack, strlen(ack)+1, 0, (struct sockaddr *)&client_addr, client_addr_len); //send ack
                    printf("ACK sent\n");
                }
                if(strcmp(split, "LOOKUP")==0){
                    // nick
                    split = strtok(NULL, splitat);
                    char *nick = malloc(strlen(split)+1);
                    strcpy(nick, split); 

                    //look for nick in list
                    client *current = head;
                    while(current != NULL){
                        if(strcmp(current->nick, nick)==0){
                            char address[INET6_ADDRSTRLEN]; //INET6_ADDRSTRLEN is the max length of an IPv6 address
                            char port[8]; // max port number is 65535
                            if (current->address.ss_family == AF_INET) {
                                struct sockaddr_in *s = (struct sockaddr_in *)&current->address; //cast the address to IPv4
                                inet_ntop(AF_INET, &s->sin_addr, address, INET6_ADDRSTRLEN); //convert the address to a string
                                sprintf(port, "%d", ntohs(s->sin_port)); //convert the port to a string
                            } else {
                                struct sockaddr_in6 *s = (struct sockaddr_in6 *)&current->address; //cast the address to IPv6
                                inet_ntop(AF_INET6, &s->sin6_addr, address, INET6_ADDRSTRLEN); //convert the address to a string
                                sprintf(port, "%d", ntohs(s->sin6_port)); //convert the port to a string
                            }

                            //make char variable for nick + address + port + tekst
                            char ack[11 + strlen(nick) + strlen(address) + strlen(port)];
                            strcpy(ack, "ACK ");
                            strcat(ack, pkt);
                            strcat(ack, " NICK ");
                            strcat(ack, nick);
                            strcat(ack, " ");
                            strcat(ack, address);
                            strcat(ack, " PORT ");
                            strcat(ack, port);
                            send_packet(sockfd, ack, strlen(ack)+1, 0, (struct sockaddr *)&client_addr, client_addr_len);
                            printf("ACK sent\n");
                            break;
                        }
                        
                        current = current->next;
                    }
                if(current == NULL){
                    char ack[13 + strlen(pkt)];
                    strcpy(ack, "ACK ");
                    strcpy(ack, pkt);
                    strcat(ack, " NOT FOUND");
                    send_packet(sockfd, ack, strlen(ack)+1, 0, (struct sockaddr *)&client_addr, client_addr_len);
                    }
                }
            }
        }            
            if (FD_ISSET(STDIN_FILENO, &fds)){
                // if server types in q then exit
                if(fgets(buf, BUF_SIZE, stdin) != NULL){
                    if(strcmp(buf, "q\n") == 0){
                        printf("Exiting The server, Good bye!\n");
                        exit(0);
                    }
                }
            }
        }
    close(sockfd);
    return EXIT_SUCCESS;
}
