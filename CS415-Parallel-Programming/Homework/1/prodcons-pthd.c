//------------------------------------------------------------------------- 
// This is supporting software for CS415/515 Parallel Programming.
// Copyright (c) Portland State University
//------------------------------------------------------------------------- 

// Producer Consumer Program using Pthreads
// Luke Waninger
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include "task-queue.h"

// function declarations
void consumer(void*);
void producer(void);

// default values
int numConsumers = 1;    // default number of consumers
int queue_cap    = 20;   // maximum number of items in the queue
int num_tasks    = 100;  // number of tasks to complete
queue_t *queue;          // the queue
pthread_mutex_t buffer;  // lock for task load/unload

struct track {
  pthread_mutex_t lock;
  int num;
};
struct track *free_count;
struct track *task_count;

void post(struct track*);
void dec(struct track*);

int main(int argc, char **argv) {
  if (argc > 1) {
    if ((numConsumers = atoi(argv[1])) < 1) {
      printf("<numCons> must be greater than 0\n");      
    }
  }
 
  // initialize the producer consumer queue
  queue = init_queue(queue_cap);

  // initialize the locks
  pthread_mutex_init(&buffer, NULL);

  struct track *free_count = malloc(sizeof(struct track));
  pthread_mutex_init(&free_count->lock, NULL);
  free_count->num = 20;

  struct track *task_count = malloc(sizeof(struct track));
  pthread_mutex_init(&task_count->lock, NULL);
  task_count->num = 0;

  // create consumer threads
  pthread_t *threads = 
    (pthread_t *) malloc(sizeof(pthread_t) * numConsumers);
  for (long k = 0; k < numConsumers; k++)
    pthread_create(&threads[k], NULL, (void*)consumer, (void*)k);

  // execute the producer code leaving time for the consumers to start
  // sleep(1);
  producer();

  // wait for consumer threads to terminate
  for (long k = 0; k < numConsumers; k++)
    pthread_join(threads[k], NULL);
  
  free(free_count);
  free(task_count);
  exit(0);
}

void producer(void) {
  printf("Producer starting on core %d\n", sched_getcpu());

  int t_num = 1;
  while(t_num <= num_tasks) {
    task_t *t = create_task(t_num, t_num);   // create task
    
    while (free_count->num == 0);
    dec(free_count);
    pthread_mutex_lock(&buffer);
    add_task(queue, t);                      // add the task
    pthread_mutex_unlock(&buffer);
    post(task_count);    
    ++t_num;
  }
  
  // lock, send termination tasks, unlock
  for (int i = 0; i < numConsumers; i++) {
    task_t *terminate = create_task(-1, -1); // create task

    while (free_count->num == 0);
    dec(free_count);
    pthread_mutex_lock(&buffer);             // lock the queue
    add_task(queue, terminate);              // add to queue
    pthread_mutex_unlock(&buffer);           // unlock the queue
    post(task_count);                        // increase count
  }
}

void consumer(void *p) {
  long k = (long)&p;
  printf("Consumer[%ld] starting on core %d\n", k, sched_getcpu());

  int t_consumed = 0;
  while (1) {
    while (task_count->num == 0);
    dec(task_count);
    pthread_mutex_lock(&buffer);              // lock the queue
    task_t *task = remove_task(queue);        // take a task
    pthread_mutex_unlock(&buffer);            // unlock
    post(free_count);

    // check for termination task
    if (task->low == -1 && task->high == -1) {
      printf("C[%ld]: %d\n", k, t_consumed);
      return;
    }

    ++t_consumed;
  }
}

void post(struct track *p) {
   pthread_mutex_lock(&p->lock);
   p->num++;
   pthread_mutex_unlock(&p->lock);
}

void dec(struct track *p) {
  pthread_mutex_lock(&p->lock);
  p->num--;
  pthread_mutex_unlock(&p->lock);
}

