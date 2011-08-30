#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

struct config {
	char *name;
	char *value;
} *configHead;

char buffer[256];
char _err[256];

char username[100];
char password[100];
char servername[100];

int file_exists(char *fileName);
int read_config(char *fileName);
void chomp(char *str);
void free_up();

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
		strcpy(_err, filename);
		strcat(_err, " does not exist\n");
		fprintf(stderr, "%s", _err);
		return 1;
	}

	int result = read_config(filename);
	if (result != 0) {
		return result;
	}

	fprintf(stderr, "%s\n", username);
	fprintf(stderr, "%s\n", password);
	fprintf(stderr, "%s\n", servername);
	free_up();

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
			return 2;
		}

		tokenized = strtok( buffer, split );
		while (tokenized != NULL) {
			// skip "set" and "=" tokens
			if ((strcmp(tokenized, "set") == 0) || (strcmp(tokenized, "=") == 0)) { }
			else {
				// 'set name = value'; if we've seen the name, then it's automagically the value
				if (c->name == NULL) {
					c->name = malloc( strlen( tokenized ));
					strcpy(c->name, tokenized);
				} else {
					c->value = malloc( strlen ( tokenized ));
					strcpy(c->value, tokenized);
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
	int n = 0;

}
