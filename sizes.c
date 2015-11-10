#include <stdio.h>

typedef struct holder {
	int type;
	void *data;
} holder; 

int main(int argc, char **argv) {
	printf("sizeof(void *): %ld\n", sizeof(void *));
	printf("sizeof(int): %ld\n", sizeof(int));
	printf("sizeof(long): %ld\n", sizeof(long));
	printf("sizeof(double): %ld\n", sizeof(double));
	printf("sizeof(char *): %ld\n", sizeof(char *));
	printf("sizeof(int(*)(void *, void *): %ld\n", sizeof(int(*)(void *, void *)));

	holder h;
	h.type = 1;
	h.data = (void *) 42;

	printf("h.data: %d\n", (int)h.data);
	printf("BUFSIZ: %d\n", BUFSIZ);

	return 0;
}