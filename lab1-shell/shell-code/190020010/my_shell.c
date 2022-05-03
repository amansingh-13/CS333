#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

typedef struct plist {
	pid_t pid;
	struct plist* next;
} plist;

plist* tail_bg = NULL;
plist* tail_fg = NULL;
char go_on = 'Y';
char** env = NULL;

char** tokenize(char* line)
{
	char** tokens = (char**) malloc(MAX_NUM_TOKENS * sizeof(char*));
	char* token = (char*) malloc(MAX_TOKEN_SIZE * sizeof(char));
	int tokenIndex = 0, tokenNo = 0;
	for(int i = 0; i < strlen(line); i++) {
		char readChar = line[i];
		if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
			token[tokenIndex] = '\0';
			if (tokenIndex != 0) {
				tokens[tokenNo] = (char*) malloc(MAX_TOKEN_SIZE * sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0;
			}
		}
		else token[tokenIndex++] = readChar;
	}
	free(token);
	tokens[tokenNo] = NULL;
	return tokens;
}

void free2d(char** ptr)
{
	for(int i = 0; ptr[i] != NULL; i++) free(ptr[i]);
	free(ptr);
}

int kill_all(plist* tail)
{
	int killed = 0;
	for(plist* it = tail; it != NULL; it = it->next) 
		if (kill(it->pid, SIGKILL) == -1);
			//fprintf(stderr, "sigkill failed for pid %d\n", it->pid);
		else killed++;
	return killed;
}

int reap_all(plist** tail, char was_killed, int options)
{
	plist* prev = NULL, *it = *tail;
	int reaped = 0;
	for(; it != NULL;) {
		int rc = waitpid(it->pid, NULL, options);
		if (rc <= 0) {
			//if(rc < 0) fprintf(stderr, "waitpid failed for pid %d\n", it->pid);
			prev = it;
			it = it->next;
			continue;
		}
		else {
			reaped++;
			if (was_killed == 'N' && options)
				fprintf(stderr, "Shell: Background process finished\n");
		}
		if (it == *tail) 
			*tail = it->next;
		else prev->next = it->next;
		plist* tmp = it;
		it = it->next;
		free(tmp);
	}
	return reaped;
}

int fork_n_exec(char** tokens, int m, int n, char bg, char pllel)
{
	if(m >= n) return -1;

	if(strcmp(tokens[m], "cd") == 0) {
		if(pllel != 'Y' && bg != 'Y') { // do nothing when run in parallel or bg
			if (n-m != 2) fprintf(stderr, "Shell: Incorrect command\n");
			else if (chdir(tokens[m+1]) < 0) fprintf(stderr, "chdir failed\n");
		}
		if (bg == 'Y') fprintf(stderr, "Shell: Background process finished\n");
		return -1;
	}
	if(strcmp(tokens[m], "exit") == 0) {
		if(pllel != 'Y' && bg != 'Y') { // do nothing when run in parallel or bg
			int killed = kill_all(tail_bg);
			while(killed != 0)
				killed -= reap_all(&tail_bg, 'Y', WNOHANG); // kill is promised, but may take some time
			free2d(tokens);
			exit(0);
		}
		if (bg == 'Y') fprintf(stderr, "Shell: Background process finished\n");
		return -1;
	}

	pid_t rv = fork();
	
	if (rv < 0); //fprintf(stderr, "fork failed with status code %d\n", rv);
	else if (rv == 0) {
		int err = 0;
		char* save = tokens[n]; // we need to restore it later for freeing correctly
		tokens[n] = NULL;
		if (setpgid(0, 0) == -1) // just to replicate bash behaviour
			fprintf(stderr, "setpgid failed\n");
		else {
			err = execvpe(tokens[m], tokens+m, env);
			fprintf(stderr, "execve failed with status code %d\n", err);
		}
		tokens[n] = save;
		free2d(tokens); // cant be double free as we are in the child process
		exit(err);
	}
	return (int) rv; // return to parent
}

void fgs(char** tokens)
{
	int rc, m = 0, n = 0;
	for(n = 0; tokens[n] != NULL && go_on == 'Y'; n++) {
		if(strcmp(tokens[n], "&&") == 0) {
			int rv = fork_n_exec(tokens, m, n, 'N', 'N');
			if (rv >= 0) {
				plist* newp = (plist*) malloc(sizeof(plist));
				newp->pid = (pid_t) rv;
				newp->next = tail_fg;
				tail_fg = newp;
				reap_all(&tail_fg, 'N', 0);
			}
			m = n+1;
		}
	}
	if (go_on != 'Y') return;
	int rv = fork_n_exec(tokens, m, n, 'N', 'N');
	if (rv >= 0) {
		plist* newp = (plist*) malloc(sizeof(plist));
		newp->pid = (pid_t) rv;
		newp->next = tail_fg;
		tail_fg = newp;
		reap_all(&tail_fg, 'N', 0);
	}
	return;
}

void fgp(char** tokens)
{
	int rc, m = 0, n = 0;
	for(n = 0; tokens[n] != NULL; n++) {
		if(strcmp(tokens[n], "&&&") == 0) {
			int rv = fork_n_exec(tokens, m, n, 'N', 'Y');
			if (rv >= 0) {
				plist* newp = (plist*) malloc(sizeof(plist));
				newp->pid = (pid_t) rv;
				newp->next = tail_fg;
				tail_fg = newp;
			}
			m = n+1;
		}
	}                                                      /// assumes correct formatting "--- &&& --- &&& ---"
	int rv = fork_n_exec(tokens, m, n, 'N', 'Y');
	if (rv >= 0) {
		plist* newp = (plist*) malloc(sizeof(plist));
		newp->pid = (pid_t) rv;
		newp->next = tail_fg;
		tail_fg = newp;
	}
	reap_all(&tail_fg, 'N', 0);
	return;
}

void bg(char** tokens)
{
	// add to plist
	int i = 0;
	for(; tokens[i] != NULL; i++);                         /// assumes correct formatting "--- &"
	int rv = fork_n_exec(tokens, 0, i-1, 'Y', 'N');
	if (rv >= 0) {
		plist* newp = (plist*) malloc(sizeof(plist));
		newp->pid = (pid_t) rv;
		newp->next = tail_bg;
		tail_bg = newp;
	}
	return;
}

void int_handler(int sig)
{
	kill_all(tail_fg);
	reap_all(&tail_fg, 'Y', 0);
	go_on = 'N';
}

int main(int argc, char** argv, char** envp)
{
	env = envp; // we may add env variables in our shell during execution
	char line[MAX_INPUT_SIZE];
	char** tokens;

	signal(SIGINT, &int_handler);

	while(1) {
		reap_all(&tail_bg, 'N', WNOHANG);
		go_on = 'Y';

		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		printf("$ ");
		scanf("%[^\n]", line);
		getchar();

		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; // terminate with new line
		tokens = tokenize(line);
		
		if (!tokens[0]) { // empty input
			free2d(tokens);
			continue;
		}

		int not_found = 1;
		for(int i = 0; tokens[i] != NULL && not_found; i++) {
			if (strcmp(tokens[i], "&") == 0) {
				bg(tokens);
				not_found = 0;
			}
			else if (strcmp(tokens[i], "&&&") == 0) {
				fgp(tokens);
				not_found = 0;
			}
		}
		if(not_found) fgs(tokens);
		
		// Freeing the allocated memory
		free2d(tokens);
	}
	return 0;
}

