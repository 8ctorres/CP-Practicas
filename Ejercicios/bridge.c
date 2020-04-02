/* Un puente antiguo de ancho reducido solo permite el tráfico simultáneo de vehículos en
   un sentido. Se desea diseñar un sistema que controle el acceso al puente de forma que un
   vehículo espera a la entrada si hay vehículos cruzando en sentido opuesto.
   Cada vehículo está representado por un thread que ejecuta la función car, y recibe como
   parámetro la dirección en la que cruza (int 1 o 0). */

#define op(dir) (((dir)+1)%2)

/*APARTADO A (CON CONDICIONES)*/

mutex_t m;
int crossing[2];
cond_t w[2]; //Waiting to cross in dir

void enter_bridge(int direction){
  lock(m);
  while(crossing[op(dir)]> 0)
    wait(w[dir],m);
  //Now there are no cars crossing in the other direction
  crossing[dir]++;
  //Now I am crossing in my direction
  unlock(m);
}

void exit_bridge(int direction){
  lock(m);
  if(--crossing[dir]) //I exit the brigde now and decrement the counter. If i am the last one, broadcast that the bridge is free
    broadcast(w[op(dir)]);
  unlock(m);
}

void *car(void *arg){
  int direction = (int *) *arg;
  enter_bridge(direction);
  //Cross the bridge
  exit_bridge(direction);
}

/*SOLUCIÓN DE JAVIER PARÍS AL APARTADO A, CON DOS MUTEX*/

mutex_t m_cars[2];
int cars[2];
mutex_t bridge;

void enter_bridge(int dir){
  lock(m_cars[dir]);
  if(cars[dir]==0){
    lock(bridge);
  }
  cars[dir]++;
  unlock(m_cars[dir]);
}

void exit_bridge(int dir){
  lock(m_cars[dir]);
  cars[dir]--;
  if(car[dir]==0){
    unlock(bridge);
  }
  unlock(m_cars[dir]);
}

/*SOLUCIÓN DE JAVIER PARÍS AL APARTADO A, CON CONDICIONES*/

int cars[2];
mutex_t m_bridge;
cond_t bridge_in_use;

void enter_bridge(int dir){
  lock(m_bridge);
  while(cars[op(dir)]>0)
    wait(bridge_in_use, m_bridge);
  cars[dir]++;
  unlock(m_bridge);
}

void exit_bridge(int dir){
  lock(m_bridge);
  cars[dir]--;
  if (cars[dir]==0)
    broadcast(bridge_in_use);
  unlock(m_bridge);
}

/*********************************************************************************/
/*********************************************************************************/

/*APARTADO B*/

#define op(dir) (((dir)+1)%2)
mutex_t m;
int crossing[2];
cond_t w[2]; //Waiting to cross in dir

void enter_bridge(int dir){
  lock(m);
  while ((crossing[op(dir)]>0) || (crossing[dir] == MAX_CARS))
    wait(w[dir], m);
  //Now there are no cars waiting in the opposite direction, AND there is capacity for one more in my direction
  crossing[dir]++;
  //Now I am crossing in my direction
  unlock(m);
}

void exit_bridge(int dir){
  lock(m);
  crossing[dir]--;
  if(crossing[dir] == 0) //I exit the bridge and decrement the counter. If I am the last one, broadcast that the brigde is free
    broadcast(w[op(dir)]); //Tell those who where waiting in the opposite direction that they can now cross
  else if(crossing[dir] == MAX_CARS-1)
    broadcast(w[dir]); //If the bridge was full, wake up cars that were waiting to cross in my direction
  unlock(m);
}

void *car(void *arg){
  int direction = (int *) *arg;
  enter_bridge(direction);
  //Cross the bridge
  exit_bridge(direction);
}

//NOTE
/*This implementation gives absolute preference to the direction that the cars inside the brigde are going in. This could cause starvation of
  one car waiting if there is a continuous traffic in the other direction.*/

/**********************************************************************************/
/**********************************************************************************/

/*APARTADO C*/

mutex_t m;
int crossing[2];
cond_t w[2]; //Waiting to cross in dir
sem_t sem[2] = MAX_CARS;

void enter_bridge(int direction){
  lock(m);
  while(crossing[op(dir)])
    wait(w[dir],m);
  //Now there are no cars crossing in the opposite dir
  unlock(m);
  p(sem[dir]);
}

void exit_bridge(int direction){
  lock(m);
  if(--crossing[dir] == 0) //I exit the bridge now and decrement my counter. If I am the last one, broadcast that the bridge is now free
    broadcast(w[op(dir)]); //Tells those who were waiting to cross in the other direction that they now can
  unlock(m);
  v(sem[dir]);
}

void *car(void *arg){
  int direction = *((int *) arg);
  enter_bridge(direction);
  //Cross the bridge
  exit_bridge(direction);
}
