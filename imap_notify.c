#include <arpa/inet.h>
#include <ctype.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define RCVBUFSIZE 1000   /* Size of receive buffer */

struct config {
	char *name;
	char *value;
} *configHead;

int stage = 0;
int commandId = 0;
char commandBuff[500];
char buffer[256];
char _err[256];

char username[100];
char password[100];
char servername[100];

int check_for_message(char *message, char *commsBuffer);
int file_exists(char *fileName);
int is_numeric(const char *s);
int read_config(char *fileName);
int recv_message(int sock, char *commsBuffer);
int sockfd;
void chomp(char *str);
void communicate();
void connect_to_server();
void free_up();
void handle_message(char *commsBuffer);
void send_message(int sock, char *msg);

int main(int argc, char *argv[]) {
	char *filename = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <mutt_config>\n",
						argv[0]);
		exit(1);
	}
	filename = argv[1];

	int exists = file_exists(filename);

	if (! exists) {
		fprintf(stderr, "%s does not exist\n", filename);
		return 1;
	}

	int result = read_config(filename);
	if (result != 0) {
		return result;
	}

	connect_to_server();
	communicate();

	close(sockfd);
	free_up();
	
	return 0;
}

int file_exists(char * fileName) {
	int exists = access(fileName, R_OK);

	if (exists == 0) {
		return 1;
	} else {
		return 0;
	}
}

void chomp(char *str) {
	int n;

	for (n=0; n<strlen(str); n++) {
		switch(*(str+n)) {
			case 0x0D:
				*(str+n)=0;
				break;
			case 0x0A:
				*(str+n)=0;
				break;
		}
	}
	*(str+n)=0;     // ensure ASCIZ even if no CRLF
}

int read_config(char *fileName) {
	FILE *imaprc;
	char *mode = "r";
	char split[] = " ";
	char *tokenized = NULL;
	char *valueTok = NULL;

	imaprc = fopen(fileName, mode);

	if (imaprc == NULL) {
		fprintf(stderr, "Can't open %s\n", fileName);
		return 1;
	}

	while (! feof(imaprc)) {
		fgets(buffer, 256, imaprc);

		chomp(buffer);

		struct config *c;

		// allocate a chunk of memory for the config struct
		c = malloc( sizeof( struct config ) );
		if (c == NULL) {
			fprintf(stderr, "Couldn't allocate memory for the config object\n");
			return 2;
		}

		tokenized = strtok( buffer, split );
		while (tokenized != NULL) {
			// skip "set" and "=" tokens
			if ((strcmp(tokenized, "set") == 0) || (strcmp(tokenized, "=") == 0)) { }
			else {
				// 'set name = value'; if we've seen the name,
				// then it's automagically the value
				if (c->name == NULL) {
					c->name = malloc( strlen( tokenized ));
					if (c->name == NULL) {
						fprintf(stderr, "Couldn't allocate memory for the name\n");
						return 2;
					}
					strcpy(c->name, tokenized);
				} else {

					valueTok = strtok( tokenized, "'" );

					// the strings are different
					if (strcmp(tokenized, valueTok) != 0) {
						c->value = malloc( strlen ( valueTok ) );
						if (c->value == NULL) {
							fprintf(stderr, "Couldn't allocate memory for the value\n");
							return 2;
						}
						strcpy(c->value, valueTok);
					} else {
						// this config option was not surrounded by '
						// so tokenize with {, then }

						valueTok = strtok( tokenized, "{" );
						valueTok = strtok( valueTok, "}" );

						if (strcmp(tokenized, valueTok) != 0) {
							c->value = malloc( strlen ( valueTok ));
							if (c->value == NULL) {
								fprintf(stderr, "Couldn't allocate memory for the value\n");
								return 2;
							}
							strcpy(c->value, valueTok);
						}
					}
				}
			}
			// tokenize the string again
			tokenized = strtok(NULL, split);
		}

		if (strcmp(c->name, "imap_login") == 0) {
			strcpy(username, c->value);
		} else if (strcmp(c->name, "imap_pass") == 0) {
			strcpy(password, c->value);
		} else if (strcmp(c->name, "mailboxes") == 0) {
			strcpy(servername, c->value);
		}

		free(c);
	}

	fclose(imaprc);

	return 0;
}

void free_up() {
	// int n = 0;

}

void connect_to_server() {
	char *port = "143";
	struct addrinfo hints, *servinfo, *p;
	int rv;
	
	fprintf(stderr, "Connecting to server\n");

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// resolve the name to an IP
	if ((rv = getaddrinfo(servername, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			fprintf(stderr, "socket\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			fprintf(stderr, "connect\n");
			continue;
		}

		fprintf(stderr, "successfully connected\n");
		break; // if we get here, we must have connected successfully
	}

	if (p == NULL) {
		fprintf(stderr, "failed to connect\n");
		exit(2);
	}

	fprintf(stderr, "Done connecting\n");

	freeaddrinfo(servinfo);

}

void communicate() {
	int bytesRcvd, totalBytesRcvd;;
	char commsBuffer[RCVBUFSIZE];
	bytesRcvd = 1;

	while (bytesRcvd != 0) {
		bytesRcvd = recv_message(sockfd, commsBuffer);
		totalBytesRcvd += bytesRcvd;
		commsBuffer[bytesRcvd] = '\0';

		handle_message(commsBuffer);
		usleep(500);
	}

	fprintf(stderr, "Received a total of %d bytes\n", totalBytesRcvd);
}

void send_message(int sock, char *msg) {
	int msgLen = 0;

	msgLen = strlen(msg);

	fprintf(stderr, "Sending %s", msg);

	if (send(sock, msg, msgLen, 0) != msgLen) {
		fprintf(stderr, "send() sent a different number of bytes than expected\n");
		exit(1);
	}
}

int recv_message(int sock, char *commsBuffer) {
	int bytesRcvd;

	if ((bytesRcvd = recv(sock, commsBuffer, RCVBUFSIZE - 1, 0)) <= 0) {
		fprintf(stderr, "recv() failed or connection closed prematurely\n");
		exit(1);
	}

	// fprintf(stderr, "Received %s\n", commsBuffer);
	return bytesRcvd;
}

void handle_message(char *commsBuffer) {
	char *tokenized;
	char *split = " ";

	if ((check_for_message("[HEADER.FIELDS (DATE SUBJECT FROM TO)]", commsBuffer) == 0) && (stage == 5)) {
		fprintf(stderr, "COMMAND: %s\n", commsBuffer);
	} else if ((check_for_message("UID FETCH completed", commsBuffer) == 0) && (stage == 4)) {
		stage++;
		tokenized = strtok( commsBuffer, "\n" );
		while (tokenized != NULL) {

			if (check_for_message("\\Seen", tokenized) == 0) { /* this message has been seen */
			} else {
				char *msgToken;
				msgToken = strtok( tokenized, " ");
				while (msgToken != NULL) {
					if (is_numeric(msgToken)) {
						
						sprintf(commandBuff, "%d UID FETCH %s BODY.PEEK[HEADER.FIELDS (Date Subject From To)]\n", ++commandId, msgToken);
						send_message(sockfd, commandBuff);

					}
					msgToken = strtok(NULL, " ");
				}
			}
			tokenized = strtok(NULL, "\n");
		}
	} else if (check_for_message("SEARCH completed", commsBuffer) == 0) {
		stage++;

		sprintf(commandBuff, "%d UID FETCH ", ++commandId);

		chomp(commsBuffer);

		tokenized = strtok( commsBuffer, split );
		while (tokenized != NULL) {
			if (is_numeric(tokenized)) {
				sprintf(commandBuff, "%s%s,", commandBuff, tokenized);
			}
			tokenized = strtok( NULL, split );
		}

		// remove trailing comma
		commandBuff[strlen(commandBuff)-1] = '\0';

		sprintf(commandBuff, "%s FLAGS\n", commandBuff);
		send_message(sockfd, commandBuff);
	} else if (check_for_message("SELECT completed", commsBuffer) == 0) {
		stage++;
		sprintf(commandBuff, "%d UID SEARCH ALL\n", ++commandId);
		send_message(sockfd, commandBuff);
	} else if (check_for_message("LOGIN completed", commsBuffer) == 0) {
		stage++;
		sprintf(commandBuff, "%d SELECT INBOX\n", ++commandId);
		send_message(sockfd, commandBuff);
	} else if (check_for_message("* OK", commsBuffer) == 0) {
		stage++;
		sprintf(commandBuff, "%d LOGIN \"%s\" %s\n", ++commandId, username, password);
		send_message(sockfd, commandBuff);
	} else {
	}
}

int check_for_message(char *message, char *commsBuffer) {
	int messageLen;
	messageLen = strlen(message);

	char *messageFound;

	messageFound = strstr(commsBuffer, message);
	if (messageFound == NULL) {
		return 1;
	} else {
		return 0;
	}
}

int is_numeric(const char *s) {
	if (s == NULL || *s == '\0' || isspace(*s))
		return 0;

	char *p;
	strtod(s, &p);
	return *p == '\0';
}
