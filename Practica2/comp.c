#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024) //1 megabyte
#define QUEUE_SIZE 20

#define COMPRESS 1
#define DECOMPRESS 0

#define _GNU_SOURCE

typedef struct {
  queue in;
  queue out;
  chunk (*process)(chunk);
  sem_t * sem_remaining_chunks;
  sem_t * q_in_available_chunks;
  sem_t * q_out_available_chunks;
  sem_t * q_in_free_spaces;
  sem_t * q_out_free_spaces;
} workerargs;

typedef struct {
  queue in;
  sem_t * q_in_available_chunks;
  sem_t * q_in_free_spaces;
  struct options opt;
  int fd,chunks;
} readercargs;

typedef struct {
  queue in;
  sem_t * q_in_available_chunks;
  sem_t * q_in_free_spaces;
  archive ar;
} readerdargs;

typedef struct {
  int chunks;
  sem_t *q_out_available_chunks;
  sem_t * q_out_free_spaces;
  queue out;
  archive ar;
} writercargs;

typedef struct {
  queue out;
  sem_t * q_out_available_chunks;
  sem_t * q_out_free_spaces;
  archive ar;
  int fd;
} writerdargs;


// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void * worker(void * arg) {
    chunk ch, res;
    workerargs * args = arg;

    while (sem_trywait(args->sem_remaining_chunks) == 0) {
        sem_wait(args->q_in_available_chunks);
        ch = q_remove(args->in);
        sem_post(args->q_in_free_spaces);

        res = (args->process)(ch);
        free_chunk(ch);

        sem_wait(args->q_out_free_spaces);
        q_insert(args->out, res);
        sem_post(args->q_out_available_chunks);
    }

    //When sem_remaining_chunks semaphore reaches 0, it means that all chunks have been processed

    pthread_detach(pthread_self());

    return NULL;
}

// this function reads a non-compressed file and inserts its data as chunks into a queue
// or takes chunks for a compressed file and puts them into a queue
void * readerc(void * arg) {
  readercargs * args = arg;
  chunk ch;
  int offset,i = 0;
  // read input file and send chunks to the in queue
  do {
    ch = alloc_chunk(args->opt.size);

    offset=lseek(args->fd, 0, SEEK_CUR);

    ch->size   = read(args->fd, ch->data, args->opt.size);
    ch->num    = i++; //Increases by iteration
    ch->offset = offset;

    sem_wait(args->q_in_free_spaces);
    q_insert(args->in, ch);
    sem_post(args->q_in_available_chunks);
  } while (ch->num < (args->chunks-1));

  //Self destruction
  pthread_detach(pthread_self());
  return NULL;
}

void * readerd(void * arg) {
  int i;
  readerdargs * args = arg;
  chunk ch;
  // read chunks with compressed data
  for(i=0; i<chunks(args->ar); i++) {
    ch = get_chunk(args->ar, i);
    sem_wait(args->q_in_free_spaces);
    q_insert(args->in, ch);
    sem_post(args->q_in_available_chunks);
  }
  //Self destruction
  pthread_detach(pthread_self());
  return NULL;
}

void *writerc(void *arg){
  writercargs * args = arg;
  chunk ch;
  int i = 0;
  // send chunks to the output archive file
  for (i = 0; i < args->chunks; i++){
    sem_wait(args->q_out_available_chunks);
    ch = q_remove(args->out);
    sem_post(args->q_out_free_spaces);

    add_chunk(args->ar, ch);
    free_chunk(ch);
  }

  return NULL;
}

void * writerd(void * arg) {
  writerdargs * args = arg;
  int i;
  chunk ch;
  // write chunks from output to decompressed file
  for(i=0; i<chunks(args->ar); i++) {
    sem_wait(args->q_out_available_chunks);
    ch=q_remove(args->out);
    sem_post(args->q_out_free_spaces);
    lseek(args->fd, ch->offset, SEEK_SET);
    write(args->fd, ch->data, ch->size);
    free_chunk(ch);
  }

  return NULL;
}

// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, chunks;
    struct stat st;
    char comp_file[256];
    archive ar;
    queue in, out;

    //Threads for worker
    pthread_t * wthreads = malloc(sizeof(pthread_t) * opt.num_threads);
    //Reader and writer threads
    pthread_t thread_reader;
    pthread_t thread_writer;
    //In and Out resource semaphores (Producer-Consumer Pattern)
    sem_t in_sem;
    sem_t out_sem;
    sem_t in_free_spaces;
    sem_t out_free_spaces;
    sem_init(&in_sem,0,0);
    sem_init(&out_sem,0,0);
    sem_init(&in_free_spaces,0, opt.queue_size);
    sem_init(&out_free_spaces,0, opt.queue_size);

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }

    fstat(fd, &st);
    chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);

    if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    ar = create_archive_file(comp_file);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    //READER summon
    //Building argument structure
    readercargs rargs;
    rargs.in = in;
    rargs.fd = fd;
    rargs.chunks = chunks;
    rargs.q_in_available_chunks = &in_sem;
    rargs.q_in_free_spaces = &in_free_spaces;
    rargs.opt = opt;

    pthread_create(&thread_reader,NULL,readerc,&rargs);
    //Join here only for debug
    //pthread_join(thread_reader,NULL);

    //WORKERS summon
    //This is a semaphore that is shared among all the workers to keep track
    //of how many chunks have been processed
    sem_t sem_remaining_chunks;
    sem_init(&sem_remaining_chunks, 0, chunks);
    //Building argument structure for the new worker
    workerargs wargs;
    wargs.in = in;
    wargs.out = out;
    wargs.process = zcompress;
    wargs.sem_remaining_chunks = &sem_remaining_chunks;
    wargs.q_in_available_chunks = &in_sem;
    wargs.q_out_available_chunks = &out_sem;
    wargs.q_in_free_spaces = &in_free_spaces;
    wargs.q_out_free_spaces = &out_free_spaces;

    // compression of chunks from in to out
    int nthreads = opt.num_threads;
    for (int i = 0; i < nthreads; i++) {
      pthread_create(&wthreads[i],NULL,worker,&wargs);
    }
    /* for (int i = 0; i < nthreads;i++) { */
    /*   pthread_join(wthreads[i],NULL); */
    /* } */

    //WRITER summon
    //Builds argument structure
    writercargs wrargs;
    wrargs.chunks = chunks;
    wrargs.out = out;
    wrargs.q_out_available_chunks = &out_sem;
    wrargs.q_out_free_spaces = &out_free_spaces;
    wrargs.ar = ar;

    pthread_create(&thread_writer, NULL, writerc,&wrargs);
    //The fact that writer() is executed by another thread does not have a practical
    //purpose, writer() must end before the main thread ends.
    pthread_join(thread_writer,NULL);

    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);
    sem_destroy(&in_sem);
    sem_destroy(&out_sem);
    sem_destroy(&in_free_spaces);
    sem_destroy(&out_free_spaces);
    sem_destroy(&sem_remaining_chunks);

    free(wthreads);
}


// Decompress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the decompressed file

void decomp(struct options opt) {
    int fd;
    char uncomp_file[256];
    archive ar;
    queue in, out;

    //Threads for worker()
    pthread_t * wthreads = malloc(sizeof(pthread_t) * opt.num_threads);
    //Reader and writer threads
    pthread_t thread_reader;
    pthread_t thread_writer;
    //In and Out resource semaphores (Producer-Consumer Pattern)
    sem_t in_sem;
    sem_t out_sem;
    sem_t in_free_spaces;
    sem_t out_free_spaces;
    sem_init(&in_sem,0,0);
    sem_init(&out_sem,0,0);
    sem_init(&in_free_spaces, 0, opt.queue_size);
    sem_init(&out_free_spaces, 0, opt.queue_size);


    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);


    //READER summon
    //Building argument structure
    readerdargs rargs;
    rargs.in = in;
    rargs.q_in_available_chunks = &in_sem;
    rargs.q_in_free_spaces = &in_free_spaces;
    rargs.ar = ar;

    pthread_create(&thread_reader,NULL,readerd,&rargs);
    //Join here only for debug
    //pthread_join(thread_reader,NULL);

    //WORKERS summon
    //This is a semaphore that is shared among all the workers to keep track
    //of how many chunks have been processed
    sem_t sem_remaining_chunks;
    sem_init(&sem_remaining_chunks, 0, ar->chunks);
    //Building arg structure for the new worker
    workerargs wargs;
    wargs.in = in;
    wargs.out = out;
    wargs.process = zdecompress;
    wargs.q_in_available_chunks = &in_sem;
    wargs.q_out_available_chunks = &out_sem;
    wargs.q_in_free_spaces = &in_free_spaces;
    wargs.q_out_free_spaces = &out_free_spaces;
    wargs.sem_remaining_chunks = &sem_remaining_chunks;

    // compression of chunks from in to out
    int nthreads = opt.num_threads;
    for (int i = 0; i < 1; i++) {
      pthread_create(&wthreads[i],NULL,worker,&wargs);
    }
    /* for (int i = 0; i < nthreads;i++) { */
    /*   pthread_join(wthreads[i],NULL); */
    /* } */

    //WRITER summon
    //Builds argument structure
    writerdargs wrargs;
    wrargs.out = out;
    wrargs.q_out_available_chunks = &out_sem;
    wrargs.q_out_free_spaces = &out_free_spaces;
    wrargs.ar = ar;
    wrargs.fd = fd;

    pthread_create(&thread_writer, NULL, writerd,&wrargs);
    //The fact that writer() is executed by another thread does not have a practical
    //purpose, writer() must end before the main thread ends.
    pthread_join(thread_writer,NULL);


    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);
    sem_destroy(&in_sem);
    sem_destroy(&out_sem);
    sem_destroy(&in_free_spaces);
    sem_destroy(&out_free_spaces);
    sem_destroy(&sem_remaining_chunks);

    free(wthreads);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
