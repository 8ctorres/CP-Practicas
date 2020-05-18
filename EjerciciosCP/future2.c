/*
  Una future es una abstracción para retrasar computaciones. Imaginemos que queremos calcular c = f(a) pero que c no lo usaremos hasta mucha más adelante en el programa.
  Podemos transformar esto como future = promise (f,a) y en el uso de c como force(future).

  Implemente futures a partir del siguiente esqueleto. */

struct future{
  void *(*f)(void *);
  void *arg;
  pthread_t id;
  void *res;
  mutex_t done_m;
};

struct future *promise(void *(*f)(void *), void *arg){
  struct future *future = malloc(sizeof(struct future));
  if (!future)
    return NULL;

  future->f = f;
  future->arg = arg;
  future->done = 0;
  mutex_init(&future->done_m, NULL);

  lock(&future->done_m);
  pthread_create(&future->id, NULL, compute_future, future);

  return future;
}

void *compute_future(void *arg){
  struct future *future = (struct future *) arg;
  future->res = future->f(future->arg);
  unlock(&future->done_m);
  return NULL;
}

void *force(struct future *future){
  lock(&future->m); //If the computation hasn't ended it will wait for this mutex
  unlock(&future->m);
  return future->res;
}
