#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>

// circular array
typedef struct _queue {
  int size;
  int used;
  int first;
  void **data;
  pthread_mutex_t *mtx;
} _queue;

#include "queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));
    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size*sizeof(void *));
    q->mtx   = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(q->mtx, NULL) == -1){
      fprintf(stderr, "Error ocurred in pthread\n");
      exit(-1);
    }
    return q;
}
//Pendiente
int q_elements(queue q) {
  pthread_mutex_lock(q->mtx);
  int temp = q->used;
  pthread_mutex_unlock(q->mtx);
  return temp;
}

// EAGAIN if the queue is full, no more elementes are added
// until some previously inserted element is removed
int q_insert(queue q, void *elem) {
  pthread_mutex_lock(q->mtx);
  if (q->used == q->size) {
    errno = EAGAIN;
    return -1;
  }

  q->data[(q->first+q->used) % q->size] = elem;
  q->used++;

  pthread_mutex_unlock(q->mtx);
  return 1;
}

// EPERM if the queue is empty, the queue is left intact
// while this condition is true
void *q_remove(queue q) {
    void *res;
    pthread_mutex_lock(q->mtx);
    if (q->used == 0) {
      errno = EPERM;
      return NULL;
    }

    res = q->data[q->first];
    q->first = (q->first+1) % q->size;
    q->used--;

    pthread_mutex_unlock(q->mtx);
    return res;
}

void q_destroy(queue q) {
    pthread_mutex_destroy(q->mtx);
    free(q->mtx);
    free(q->data);
    free(q);
}
