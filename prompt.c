#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
// #include <editline/history.h>

// #define INPUT_SIZE 2048

// static char input[INPUT_SIZE];

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