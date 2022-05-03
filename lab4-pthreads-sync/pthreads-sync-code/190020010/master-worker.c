#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce, curr_buf_size, items_consumed;
int total_items, max_buf_size, num_workers, num_masters;
pthread_mutex_t buffer_l = PTHREAD_MUTEX_INITIALIZER, processor_l = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER, empty = PTHREAD_COND_INITIALIZER;

#define debug(x) do {for(int j=0; j<max_buf_size; j++) printf("%d ", x[j]); printf("\n");} while(0)

int* buffer;

void print_produced(int num, int master) {
  printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker) {
  printf("Consumed %d by worker %d\n", num, worker);
}


//produce items and place in buffer
//modify code below to synchronize correctly
void* generate_requests_loop(void* data)
{
  int thread_id = *((int*) data);
  int prev_item_to_produce;
  while(1){
    pthread_mutex_lock(&buffer_l);
    while(curr_buf_size >= max_buf_size){
      pthread_cond_wait(&empty, &buffer_l);
    }
    if(item_to_produce >= total_items){
      pthread_mutex_unlock(&buffer_l);
      break;
    }
    buffer[curr_buf_size++] = item_to_produce;
    prev_item_to_produce = item_to_produce;
    item_to_produce += 1;
    pthread_cond_broadcast(&fill);
    pthread_mutex_unlock(&buffer_l);
    
    // cleanup
    print_produced(prev_item_to_produce, thread_id);
  }
  return 0;
}

//write function to be run by worker threads
//ensure that the workers call the function print_consumed when they consume an item
void* process_requests_loop(void* data)
{
  int thread_id = *((int*) data);
  int item_to_consume;
  while(1){
    pthread_mutex_lock(&processor_l);
    if(items_consumed >= total_items){
      pthread_mutex_unlock(&processor_l);
      break; 
    }
    items_consumed++;
    pthread_mutex_unlock(&processor_l);

    pthread_mutex_lock(&buffer_l);
    while(curr_buf_size <= 0) {
      pthread_cond_wait(&fill, &buffer_l);
    }
    item_to_consume = buffer[--curr_buf_size];
    pthread_cond_broadcast(&empty);
    pthread_mutex_unlock(&buffer_l);

    // process
    print_consumed(item_to_consume, thread_id);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int *master_thread_id, *worker_thread_id;
  pthread_t *master_thread, *worker_thread;
  item_to_produce = 0;
  curr_buf_size = 0;
  items_consumed = 0;

  int i;
  
  if (argc < 5) {
    printf("./master-worker #total_items #max_buf_size #num_workers #masters e.g. ./exe 10000 1000 4 3\n");
    exit(1);
  }
  else {
    num_masters = atoi(argv[4]);
    num_workers = atoi(argv[3]);
    total_items = atoi(argv[1]);
    max_buf_size = atoi(argv[2]);
  }

  buffer = (int*) malloc(sizeof(int) * max_buf_size);

  //create master producer threads
  master_thread_id = (int*) malloc(sizeof(int) * num_masters);
  master_thread = (pthread_t*) malloc(sizeof(pthread_t) * num_masters);
  worker_thread_id = (int*) malloc(sizeof(int) * num_workers);
  worker_thread = (pthread_t*) malloc(sizeof(pthread_t) * num_workers);
  
  for (i = 0; i < num_masters; i++)
    master_thread_id[i] = i;

  for (i = 0; i < num_masters; i++)
    pthread_create(&master_thread[i], NULL, generate_requests_loop, (void*)&master_thread_id[i]);
    
  //create worker consumer threads
  for (i = 0; i < num_workers; i++)
    worker_thread_id[i] = i;
  
  for (i = 0; i < num_workers; i++)
    pthread_create(&worker_thread[i], NULL, process_requests_loop, (void*)&worker_thread_id[i]);
 
  //wait for all threads to complete
  for (i = 0; i < num_masters; i++) {
    pthread_join(master_thread[i], NULL);
    printf("master %d joined\n", i);
  }
  for (i = 0; i < num_workers; i++) {
    pthread_join(worker_thread[i], NULL);
    printf("worker %d joined\n", i);
  }
  
  //deallocating Buffers
  free(buffer);
  free(master_thread_id);
  free(master_thread);
  free(worker_thread_id);
  free(worker_thread);
  
  return 0;
}
