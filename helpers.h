#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#endif
#define BUFLEN 3000
typedef struct UDPmessage{
	unsigned long ip;
	unsigned short port;
	//char topic[51];
	//uint8_t type;
	char message[BUFLEN];
}UDPmessage;

typedef struct Subscription{
	char topic[51];
	int SF;
}Subscription;

typedef struct TCPsubscriber{
	char id[11];
	int sockfd;
	struct Subscription* abonamente;
	int nr_abonamente;
	bool connected;
}TCPsubscriber;

typedef struct Topic{
	char title[51];
	char **ids;
	int *SFuri;
	int nr_subscribers;
}Topic;

typedef struct TCPCommand{
	char id[11];
	int type;
	char topic[51];
	int SF;
}TCPCommand;

