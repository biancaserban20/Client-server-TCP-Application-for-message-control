#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

#define BUFLEN 3000

void usage(char *file)
{
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

int max(int num1, int num2)
{
	if (num1 > num2)
		return num1;

	return num2;
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	fd_set read_fds, tmp_fds;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc < 4)
	{
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// Deactivating the Nagle Algorithm
	int one = 1;
	int set = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	if (set == -1)
	{
		perror("Error in setsockopt\n");
		exit(-1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton\n");

	ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect\n");

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	int fdmax = max(sockfd, STDIN_FILENO);
	//Send the id client to the server
	n = send(sockfd, argv[1], strlen(argv[1]), 0);
	DIE(n < 0, "send id\n");

	while (1)
	{
		tmp_fds = read_fds;

		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select\n");

		if (FD_ISSET(0, &tmp_fds))
		{
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0)
			{
				// CLIENT DISCONNECTED
				close(sockfd);
				return 0;
			}
			else if (strncmp(buffer, "subscribe", 9) == 0)
			{
				//SUBSCRIBE TO TOPIC
				TCPCommand *command = (TCPCommand *)malloc(sizeof(TCPCommand));
				strcpy(command->topic, buffer + 10);
				command->topic[strlen(command->topic) - 3] = '\0';
				strcpy(command->id, argv[1]);
				command->type = 1;
				command->SF = atoi(buffer + 10 + strlen(command->topic));
				n = send(sockfd, command, sizeof(TCPCommand), 0);
				DIE(n < 0, "send");

				memset(buffer, 0, BUFLEN);
				int rc = recv(sockfd, buffer, sizeof(buffer), 0);
				if (strncmp(buffer, "1", 1) == 0)
					perror("Client already subscribed to this topic\n");
				else if (strncmp(buffer, "2", 1) == 0)
					printf("Subscribed to topic.\n");
			}
			else if (strncmp(buffer, "unsubscribe", 11) == 0)
			{
				//UNSUBSCRIBE FROM TOPIC
				TCPCommand *command = (TCPCommand *)malloc(sizeof(TCPCommand));
				strcpy(command->topic, buffer + 12);
				strcpy(command->id, argv[1]);
				command->type = 0;
				n = send(sockfd, command, sizeof(TCPCommand), 0);
				DIE(n < 0, "send\n");
				printf("Unsubscribed from topic.\n");
			}
			else
				fprintf(stderr, "No input found\n");
		}

		else if (FD_ISSET(sockfd, &tmp_fds))
		{
			memset(buffer, 0, BUFLEN);
			n = recv(sockfd, buffer, sizeof(UDPmessage), 0);
			DIE(n < 0, "recv\n");
			//printf("Am primit %d bytes\n", n);
			//printf("Mesaj primit:%s\n", buffer);

			// TCP client received an UDP message from server
			if (n == sizeof(UDPmessage))
			{
				UDPmessage *msg = (UDPmessage *)malloc(sizeof(UDPmessage));
				memcpy(msg, buffer, n);

				struct in_addr ipadress;
				ipadress.s_addr = msg->ip;

				//printf("%s:%u - ", inet_ntoa(ipadress), ntohs(msg->port));
				char topic[51];
				memcpy(topic, msg->message, 50);
				topic[50] = '\0';
				printf("%s - ", topic);
				uint8_t type;
				memcpy(&type, msg->message + 50, 1);
				//printf("Mesaj printat: ");

				if (type == 0)
				{
					printf("INT - ");
					uint8_t sign;
					memcpy(&sign, msg->message + 51, 1);
					uint32_t number;
					memcpy(&number, msg->message + 52, 4);

					if (sign == 1)
						printf("-%d\n", ntohl(number));
					else
						printf("%d\n", ntohl(number));
				}
				else if (type == 1)
				{
					printf("SHORT_REAL - ");
					uint16_t number;
					memcpy(&number, msg->message + 51, 2);
					number = ntohs(number);
					float other = (((float)number) / 100);
					uint16_t decimals = number%100;
					if(decimals == 0)
						printf("%g\n", other);
					else
						printf("%.2f\n", other);
				}
				else if (type == 2)
				{
					printf("FLOAT - ");

					uint8_t sign;
					memcpy(&sign, msg->message + 51, 1);
					uint32_t number;
					memcpy(&number, msg->message + 52, 4);
					number = ntohl(number);
					uint8_t mod;
					memcpy(&mod, msg->message + 56, 1);

					uint8_t power = mod;
					float other = ((float)number);
					while (power > 0)
					{
						other = other / 10;
						power--;
					}

					if (sign == 1)
						printf("-%.*f\n", mod, other);
					else
						printf("%.*f\n", mod, other);
				}
				else
				{
					char sir[1501];
					memcpy(sir, msg->message + 51, 1500);
					sir[1500] = '\0';
					printf("STRING - %s\n", sir);
				}
			}
			else if (n == 0)
			{
				close(sockfd);
				return 0;
			}
		}
	}

	close(sockfd);

	return 0;
}
