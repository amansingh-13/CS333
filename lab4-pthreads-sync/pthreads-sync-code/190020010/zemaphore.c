#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include "zemaphore.h"

void zem_init(zem_t *s, int value) {
  if(s){
    s->m = PTHREAD_MUTEX_INITIALIZER;
    s->c = PTHREAD_COND_INITIALIZER;
    s->val = value;
  }
}

void zem_down(zem_t *s) {
    pthread_mutex_lock(&s->m);
    while(s->val <= 0)
      pthread_cond_wait(&s->c, &s->m);
    s->val -= 1;
    pthread_mutex_unlock(&s->m);
}

void zem_up(zem_t *s) {
    pthread_mutex_lock(&s->m);
    s->val += 1;
    pthread_cond_broadcast(&s->c);
    pthread_mutex_unlock(&s->m);
    //pthread_cond_signal(&s->c);
}
