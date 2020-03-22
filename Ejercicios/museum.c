typedef struct{
  int counter;
  const int members;
  mutex_t counter_m;
  cond_t partial_grp;
} group;

shared int capacity;
shared mutex_t museum;

void enter(group grp){
  lock(grp->counter_m);
  grp->counter++;
  if(grp->counter == 1){
    lock(museum);
    while(capacity < grp->members)
      wait(no_capacity, museum);
    capacity = grp->members;
    unlock(museum);
  }
  if(grp->counter == grp->members)
    broadcast(grp->partial_grp);
  else
    wait(grp->partial_grp, grp->counter_m);
  unlock(grp->counter_m);
}

void exit(group grp){
  lock(grp->counter_m);
  grp->counter--;
  if(grp->counter==0){
    lock(museum);
    capacity+=grp->members;
    broadcast(no_capacity);
    unlock(museum);
  }
  unlock(grp->counter_m);
}
