/*
  Una future es una abstracción para retrasar computaciones. Imaginemos que queremos calcular c = f(a) pero que c no lo usaremos hasta mucha más adelante en el programa.
  Podemos transformar esto como future = promise (f,a) y en el uso de c como force(future).

  Implemente futures a partir del siguiente esqueleto. */

struct future{
  void *(*f)(void *);
  void *arg;
  pthread_t id;
  int done;
  mutex_t done_m;
  cond_t not_done;
  void *res;
};

struct future *promise(void *(*f)(void *), void *arg){
  struct future *future = malloc(sizeof(struct future));
  if (!future)
    return NULL;

  future->f = f;
  future->arg = arg;
  future->done = 0;
  mutex_init(&future->m, NULL);
  cond_t(&future->not_done, NULL);

  pthread_create(&future->id, NULL, compute_future, future);

  return future;
}

void *force(struct future *future){
  lock(&future->m);
  if(future->done == 0)
    wait(future->not_done, future->done_m);
  void *res = future->res;
  unlock(&future->m);
  return res;

void *compute_future(void *arg){
  struct future *future = (struct future *) arg;
  void *res = future->f(future->arg);
  lock(&future->done_m);
  future->done = 1;
  broadcast(future->not_done);
  future->res = res;
  unlock(&future->done_m);
}
