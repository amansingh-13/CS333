#include <pthread.h>

typedef struct zemaphore {
    pthread_mutex_t m;
    pthread_cond_t c;
    int val;
} zem_t;

void zem_init(zem_t *, int);
void zem_up(zem_t *);
void zem_down(zem_t *);
