#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

#define FD_START 4
#define client_topic 200    // clients per topic
#define max_subs 200        // max subscriptions pentru TCPsubscriber, vector de subscriptions
#define max_subscribers 200 // pentru ids si sfs din Topic

int max_clients = 100;
int max_topics = 100;

void usage(char *file)
{
    fprintf(stderr, "Usage: %s server_port\n", file);
    exit(0);
}

int max(int num1, int num2)
{
    if (num1 > num2)
        return num1;

    return num2;
}

void sendMsgtoSubscribers(UDPmessage msg, char *topic, Topic *topics, TCPsubscriber *database, int no_topics, int no_subscribers)
{
    for (int i = 0; i < no_topics; i++)
    {
        if (strcmp(topics[i].title, topic) == 0)
        {
            // found the topic needed
            for (int j = 0; j < topics[i].nr_subscribers; j++)
            {
                char id_curr[11];
                strcpy(id_curr, topics[i].ids[j]);
                int sf_curr = topics[i].SFuri[j];
                for (int k = 0; k < no_subscribers; k++)
                {
                    if (strcmp(id_curr, database[k].id) == 0 && database[k].connected == true) // modificare: am adaugat connected
                    {
                        int n = send(database[k].sockfd, &msg, sizeof(msg), 0);
                        DIE(n < 0, "send\n");
                    }
                }
            }
            break;
        }
    }
}
int main(int argc, char *argv[])
{
    int tcp_sockfd, udp_sockfd, portno;
    int no_TCPclients = 0;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFLEN];
    socklen_t clilen;

    Topic *topics = (Topic *)malloc(max_topics * sizeof(Topic));
    //Deposit *unreceived_msg = (Deposit *)malloc(1000 * sizeof(Deposit));
    TCPsubscriber *database = (TCPsubscriber *)malloc(max_clients * sizeof(TCPsubscriber));
    int no_topics = 0;
    int no_subscribers = 0;

    if (argc < 2)
    {
        usage(argv[0]);
    }

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    portno = atoi(argv[1]);
    DIE(portno < 0, "Invalid port\n");

    // Filling server information
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(portno);

    // Creating UDP socket
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket with the server address
    int rc = bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(rc < 0, "Bind failed\n");

    // Creating TCP socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "Error in socket\n");

    // Deactivating the Nagle Algorithm
    int one = 1;
    int set = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (set == -1)
    {
        perror("Error in setsockopt\n");
        exit(-1);
    }

    // Bind the socket with the server address
    int bound = bind(tcp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(bound < 0, "Bind failed\n");

    // Listen for connection requests
    int ret = listen(tcp_sockfd, 100);
    DIE(ret < 0, "Couldn't listen on socket!\n");

    fd_set read_fds; // multimea de citire folosita in select()
    fd_set tmp_fds;  // multime folosita temporar
    int fdmax;       // valoare maxima fd din multimea read_fds

    // se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET(udp_sockfd, &read_fds);
    FD_SET(tcp_sockfd, &read_fds);
    FD_SET(0, &read_fds);
    fdmax = max(udp_sockfd, tcp_sockfd);

    while (1)
    {
        tmp_fds = read_fds;

        int ret1 = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret1 < 0, "Error in select\n");

        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                //printf("Am primit de pe socketul:%d\n", i);
                if (i == 0)
                {
                    // Se citeste de la tastatura
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN, stdin);

                    // If "exit" is typed in from STDIN
                    if (strncmp(buffer, "exit", 4) == 0)
                    {
                        for (int j = 1; j <= fdmax; j++)
                            if (FD_ISSET(j, &read_fds))
                            {
                                close(j);
                                FD_CLR(j, &read_fds);
                            }
                        close(udp_sockfd);
                        close(tcp_sockfd);
                        return 0;
                    }
                    else
                        fprintf(stderr, "No input found\n");
                }
                else if (i == udp_sockfd)
                {
                    memset(&cliaddr, 0, sizeof(struct sockaddr_in));
                    clilen = sizeof(cliaddr);
                    memset(buffer, 0, BUFLEN);
                    int n = recvfrom(udp_sockfd, buffer, BUFLEN, 0, (struct sockaddr *)&cliaddr, &clilen);
                    DIE(n < 0, "Error in recvfrom\n");

                    struct UDPmessage msg;
                    msg.ip = cliaddr.sin_addr.s_addr;
                    msg.port = ntohs(cliaddr.sin_port);
                    memset(msg.message, 0, BUFLEN);
                    memcpy(msg.message, buffer, n);
                    char topic[51];
                    memcpy(topic, msg.message, 50);
                    topic[50] = '\0';

                    //is the topic alreay created?
                    bool topicfound = false;
                    for (int j = 0; j < no_topics; j++)
                    {
                        if (strcmp(topics[j].title, topic) == 0)
                        {
                            topicfound = true;
                            break;
                        }
                    }

                    // add it to the array of topics
                    if (topicfound == false)
                    {
                        strcpy(topics[no_topics].title, topic);
                        topics[no_topics].ids = (char **)malloc(client_topic * sizeof(char *));
                        for (int j = 0; j < client_topic; j++) //max_clients
                            topics[no_topics].ids[j] = (char *)malloc(11 * sizeof(char));
                        topics[no_topics].SFuri = (int *)malloc(client_topic * sizeof(int));
                        topics[no_topics].nr_subscribers = 0;
                        no_topics++;
                        if (no_topics == max_topics)
                        {
                            topics = realloc(topics, 2 * max_topics);
                            max_topics = 2 * max_topics;
                        }
                    }

                    else
                    {
                        // send the message to the subscribers
                        sendMsgtoSubscribers(msg, topic, topics, database, no_topics, no_subscribers);
                    }
                }

                else if (i == tcp_sockfd)
                {
                    // a TCP client wants to connect
                    clilen = sizeof(cliaddr);
                    int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    DIE(newsockfd < 0, "accept\n");

                    // add the new socket from accept() to the reading set
                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax)
                    {
                        fdmax = newsockfd;
                    }

                    // server receives the id
                    memset(buffer, 0, BUFLEN);
                    int n = recv(newsockfd, buffer, sizeof(buffer), 0);
                    int ok = 0;
                    int err = 0;
                    for (int j = 0; j < no_subscribers; j++)
                    {
                        if (strcmp(buffer, database[j].id) == 0 && database[j].connected == false)
                        {
                            database[j].sockfd = newsockfd;
                            database[j].connected = true;
                            ok = 1;
                            printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)); //htons
                            break;
                        }
                        else if (strcmp(buffer, database[j].id) == 0 && database[j].connected == true)
                        {
                            printf("Client %s already connected.\n", buffer);
                            close(newsockfd);
                            FD_CLR(newsockfd, &read_fds);
                            ok = 1;
                            err = 1;
                            break;
                        }
                    }

                    if (err == 1)
                        continue;
                    else if (ok == 0 && err == 0)
                    {
                        // add a new subcriber (client)
                        strcpy(database[no_subscribers].id, buffer);
                        database[no_subscribers].sockfd = newsockfd;
                        database[no_subscribers].connected = true;
                        database[no_subscribers].nr_abonamente = 0;
                        database[no_subscribers].abonamente = (Subscription *)malloc(max_subs * sizeof(Subscription));
                        no_subscribers++;
                        if (no_subscribers == max_clients)
                        {
                            database = (TCPsubscriber *)realloc(database, 2 * max_clients * sizeof(TCPsubscriber));
                            max_clients = 2 * max_clients;
                        }
                        printf("New client %s connected from %s:%d.\n", buffer, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)); //htons
                    }
                }
                else
                {
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    memset(buffer, 0, BUFLEN);
                    int n = recv(i, buffer, BUFLEN, 0);
                    DIE(n < 0, "recv\n");

                    //client disconnected
                    if (n == 0)
                    {
                        for (int j = 0; j < no_subscribers; j++)
                        {
                            if (database[j].sockfd == i && database[j].connected == true)
                            {
                                database[j].connected = false;
                                printf("Client %s disconnected.\n", database[j].id);
                                close(i);
                                break;
                            }
                        }
                        FD_CLR(i, &read_fds);
                    }
                    else
                    {
                        TCPCommand *command = (TCPCommand *)buffer;
                        //SUBSCRIBE
                        if (command->type == 1)
                        {
                            int found = 0;
                            for (int j = 0; j < no_topics; j++)
                            {
                                int ok = 0;
                                //topic found
                                if (strcmp(topics[j].title, command->topic) == 0)
                                {
                                    found = 1;
                                    for (int k = 0; k < topics[j].nr_subscribers; k++)
                                    {
                                        if (strcmp(topics[j].ids[k], command->id) == 0)
                                        {
                                            //client already subscribed
                                            ok = 1;
                                            break;
                                        }
                                    }
                                    if (ok == 0)
                                    {
                                        //add the subscriber to the topic
                                        strcpy(topics[j].ids[topics[j].nr_subscribers], command->id);
                                        topics[j].SFuri[topics[j].nr_subscribers] = command->SF;
                                        topics[j].nr_subscribers++;
                                        if (topics[j].nr_subscribers % max_subscribers == 0)
                                        {
                                            topics[j].ids = (char **)realloc(topics[j].ids, (topics[j].nr_subscribers + max_subscribers) * sizeof(char *));
                                            topics[j].SFuri = (int *)realloc(topics[j].SFuri, (topics[j].nr_subscribers + max_subscribers) * sizeof(int));
                                            for (int m = topics[j].nr_subscribers; m < topics[j].nr_subscribers + max_subscribers; m++)
                                            {
                                                topics[j].ids[m] = (char *)malloc(11 * sizeof(char));
                                            }
                                        }
                                        // add the subscription to the TCP subscriber
                                        for (int k = 0; k < no_subscribers; k++)
                                        {
                                            if (database[k].sockfd == i && database[k].connected == true)
                                            {
                                                strcpy(database[k].abonamente[database->nr_abonamente].topic, command->topic);
                                                database[k].abonamente[database->nr_abonamente].SF = command->SF;
                                                database[k].nr_abonamente++;
                                                if (database[k].nr_abonamente % max_subs == 0)
                                                {
                                                    database[k].abonamente = (Subscription *)realloc(database[k].abonamente, (database[k].nr_abonamente + max_subs) * sizeof(Subscription));
                                                }
                                            }
                                        }
                                        ok = 2;
                                    }
                                    if (ok == 1)
                                    {
                                        //printf("OK A FOST 1\n");
                                        n = send(i, "1", 2, 0);
                                        DIE(n < 0, "send\n");
                                    }
                                    else if (ok == 2)
                                    {
                                        //printf("OK A FOST 2\n");
                                        n = send(i, "2", 2, 0);
                                        DIE(n < 0, "send\n");
                                    }
                                    //printf("OK A FOST 0\n");
                                }
                            }
                            if (found == 0)
                            {
                                strcpy(topics[no_topics].title, command->topic);
                                topics[no_topics].nr_subscribers = 1;

                                topics[no_topics].ids = (char **)malloc(client_topic * sizeof(char *));
                                for (int j = 0; j < max_clients; j++)
                                    topics[no_topics].ids[j] = (char *)malloc(11 * sizeof(char));
                                topics[no_topics].SFuri = (int *)malloc(client_topic * sizeof(int));
                                topics[no_topics].SFuri[0] = command->SF;
                                strcpy(topics[no_topics].ids[0], command->id);
                                no_topics++;
                                if (no_topics == max_topics)
                                {
                                    topics = realloc(topics, 2 * max_topics);
                                    max_topics = 2 * max_topics;
                                }
                                n = send(i, "2", 2, 0);
                                DIE(n < 0, "send\n");
                            }
                        }
                        else if (command->type == 0)
                        {
                            int found = 0;
                            for (int j = 0; j < no_topics; j++)
                            {
                                //topic found
                                if (strcmp(topics[j].title, command->topic) == 0)
                                {
                                    found = 1;
                                    if (topics[j].nr_subscribers == 1)
                                        topics[j].nr_subscribers = 0;
                                    for (int k = 0; k < topics[j].nr_subscribers; k++)
                                    {
                                        if (strcmp(topics[j].ids[k], command->id) == 0)
                                        {
                                            //client already subscribed
                                            strcpy(topics[j].ids[k], topics[j].ids[topics->nr_subscribers - 1]);
                                            topics[j].SFuri[j] = topics[j].SFuri[topics[j].nr_subscribers];
                                            topics[j].nr_subscribers--;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close(udp_sockfd);
    close(tcp_sockfd);

    return 0;
}