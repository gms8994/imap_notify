#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

struct config {
	char *name;
	char *value;
} *configHead;

struct config *configs[50];
int configsIndex = 0;

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


main() {
	FILE *imaprc;
	char *mode = "r";
	char buffer[256];
	char _err[256];
	char *tokenized = NULL;
	char split[] = " ";

	char *filename = "/home/glens/.mutt.imap";

	int exists = file_exists(filename);

	if (! exists) {
		strcpy(_err, filename);
		strcat(_err, " does not exist\n");
		fprintf(stderr, "%s", _err);
		return 1;
	}

	imaprc = fopen(filename, mode);

	if (imaprc == NULL) {
		strcpy(_err, "Can't open ");
		strcat(_err, filename);
		strcat(_err, "\n");
		fprintf(stderr, "%s", _err);
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
		configs[configsIndex++] = c;
	}

	fclose(imaprc);

	// print the values from the configs struct
	int n = 0;
	for(n = 0; n < configsIndex; n++) {
		printf("%s = %s\n", configs[n]->name, configs[n]->value);
	}

	// free up our memory
	for(n = 0; n < configsIndex; n++) {
		if (configs[n] != NULL) {
			if (configs[n]->name != NULL)
				free(configs[n]->name);
			if (configs[n]->value != NULL)
				free(configs[n]->value);

			free(configs[n]);
		}
	}
}
