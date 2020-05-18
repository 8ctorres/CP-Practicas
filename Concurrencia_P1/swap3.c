#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "options.h"

struct buffer {
  int *data;
  int size;
  pthread_mutex_t *mtxarray;
};

struct thread_info {
  pthread_t       thread_id;        // id returned by pthread_create()
  int             thread_num;       // application defined thread #
};

struct args {
  int     thread_num;         // application defined thread #
  int           delay;        // delay between operations
  int		*iterations;      //!! total number of iterations
  pthread_mutex_t * itmtx;    //!! mutex for the concurrent access of threads to the global iteration counter
  struct buffer   *buffer;    // Shared buffer
};

int countdown(int * it,pthread_mutex_t * mtx) {
  int temp;
  pthread_mutex_lock(mtx);
  temp = *it ? (*it)-- : 0;
  pthread_mutex_unlock(mtx);
  return temp;
}

void *swap(void *ptr)
{
  struct args *args =  ptr;
  //countdown returns true if n != 0 (if &n corresponds to the first argument) and sustracts 1 from n
  while(countdown(args->iterations,args->itmtx)) {
    int i,j, tmp;

    i=rand() % args->buffer->size;
    j=rand() % args->buffer->size;

    if(i!=j){
      if (j>i) {
        pthread_mutex_lock(&args->buffer->mtxarray[i]);
        pthread_mutex_lock(&args->buffer->mtxarray[j]);
      }
      else {
        pthread_mutex_lock(&args->buffer->mtxarray[j]);
        pthread_mutex_lock(&args->buffer->mtxarray[i]);
      }
    }else
      pthread_mutex_lock(&args->buffer->mtxarray[j]);

    printf("Thread %d swapping positions %d (== %d) and %d (== %d)\n",
           args->thread_num, i, args->buffer->data[i], j, args->buffer->data[j]);

    tmp = args->buffer->data[i];
    if(args->delay) usleep(args->delay); // Force a context switch

    args->buffer->data[i] = args->buffer->data[j];
    if(args->delay) usleep(args->delay);

    args->buffer->data[j] = tmp;
    if(args->delay) usleep(args->delay);

    /*Seems unnecesary but without this disjunction
      the critical zone can be accessed simultaneously
      by more that one thread
      \/\/\/
     */
    if (i!=j) {
      pthread_mutex_unlock(&args->buffer->mtxarray[i]);
      pthread_mutex_unlock(&args->buffer->mtxarray[j]);
    }
    else pthread_mutex_unlock(&args->buffer->mtxarray[i]);
  }
  return NULL;
}

void print_buffer(struct buffer buffer) {
  int i;

  for (i = 0; i < buffer.size; i++)
    printf("%i ", buffer.data[i]);
  printf("\n");
}

void start_threads(struct options opt)
{
  int i;
  struct thread_info *threads;
  struct args *args;
  struct buffer buffer;

  srand(time(NULL));

  if((buffer.data=malloc(opt.buffer_size*sizeof(int)))==NULL) {
    printf("Out of memory\n");
    exit(1);
  }
  buffer.size = opt.buffer_size;

  for(i=0; i<buffer.size; i++)
    buffer.data[i]=i;

  if ((buffer.mtxarray = malloc(sizeof(pthread_mutex_t)*(buffer.size)))==NULL){
    printf("Not enough memory\n");
    exit(1);
  } //se crea el array de MUTEXs

  for(i=0; i<buffer.size; i++)
    pthread_mutex_init(&buffer.mtxarray[i], NULL);

  printf("creating %d threads\n", opt.num_threads);
  threads = malloc(sizeof(struct thread_info) * opt.num_threads);
  args = malloc(sizeof(struct args) * opt.num_threads);
  //El contador regresivo de iteraciones se guarda en la direccion dada
  //por el siguiente malloc
  int *global_iters = malloc(sizeof(int));
  //Preparación del mutex del contador global:
  pthread_mutex_t * gl_it_mtx = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(gl_it_mtx,NULL);


  if (threads == NULL || args == NULL || global_iters == NULL) {
    printf("Not enough memory\n");
    exit(1);
  }

  *global_iters = opt.iterations;

  printf("Buffer before: ");
  print_buffer(buffer);

  // Create num_thread threads running swap()
  for (i = 0; i < opt.num_threads; i++) {
    threads[i].thread_num = i;

    args[i].thread_num = i;
    args[i].buffer     = &buffer;
    args[i].delay      = opt.delay;
    args[i].iterations = global_iters;
    args[i].itmtx      = gl_it_mtx;

    if ( 0 != pthread_create(&threads[i].thread_id, NULL,
           swap, &args[i])) {
      printf("Could not create thread #%d", i);
      exit(1);
    }
  }

  // Wait for the threads to finish
  for (i = 0; i < opt.num_threads; i++)
    pthread_join(threads[i].thread_id, NULL);

  // Print the buffer
  printf("Buffer after:  ");
  print_buffer(buffer);

  for(i=0; i<buffer.size; i++)
    pthread_mutex_destroy(&buffer.mtxarray[i]);

  pthread_mutex_destroy(gl_it_mtx);

  free(args);
  free(threads);
  free(buffer.data);
  free(buffer.mtxarray);
  free(gl_it_mtx);

  pthread_exit(NULL);
}

int main (int argc, char **argv)
{
  struct options opt;

  // Default values for the options
  opt.num_threads = 10;
  opt.buffer_size = 10;
  opt.iterations  = 10;
  opt.delay       = 10;

  read_options(argc, argv, &opt);

  start_threads(opt);

  exit (0);
}
