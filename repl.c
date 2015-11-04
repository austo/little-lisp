#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <string.h>

enum { BUFFER_SIZE 2048 };

static char buffer[BUFFER_SIZE];

char *readline(char *prompt) {
	fputs(prompt, stdout);
	fgets(buffer, BUFFER_SIZE, stdin);
	char *cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif

int main(int argc, char** argv) {
	puts("Lispy Version 0.0.0.0.1");
	puts("Press Ctrl+c to exit\n");

	while(1) {
		char *input = readline("lispy> ");
		add_history(input);
		printf("No, you're a %s\n", input);
		free(input);
 }
 return 0;
}