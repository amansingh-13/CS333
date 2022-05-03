#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>


void inf(){
	char* l[] = {"/bin/sleep", "1000", NULL};
	fprintf(stderr, "%d\n", execve(l[0], l, NULL));
}

void mp(int n){
	while(n--){
		int rv = fork();
		if(rv == 0) inf();
	}
}

void handle(int sig){ return; }

int main(){
	signal(SIGINT, &handle);
	mp(5);
	while(1);
}
