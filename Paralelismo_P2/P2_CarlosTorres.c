//Autores:
//  Carlos Torres Paz - 54225269D
//  Daniel Sergio Vega Rodríguez - 34277051J

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int MPI_FlattreeReduce(void * send_data, void* recv_data, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm communicator) {
  int rank,numprocs,k;
  int recbuff;
  int * acc = recv_data;
  MPI_Status status;

  MPI_Comm_size(communicator,&numprocs);
  MPI_Comm_rank(communicator,&rank);


  if (rank == root) {
    //Receives count from itself
    *acc = *(int *)send_data;
    //Receives count from all other processes and adds them to its own count
    for (k=1; k<numprocs; k++){
      MPI_Recv(&recbuff, count, datatype, MPI_ANY_SOURCE, MPI_ANY_TAG, communicator, &status);
      *acc += recbuff;
    }
  }
  else{
    MPI_Send(send_data, count, datatype, root, rank, communicator);
  }
  return 0;
};

//Generic integer exponentiation function
int ipow(int base, int exp)
{
  int result = 1;
  while (1)
    {
      //Even exp
      if (exp & 1)
        result *= base;
      //Next digit
      exp >>= 1;
      if (!exp)
        break;
      base *= base;
    }

  return result;
}

int MPI_BinomialtreeBcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
  int rank,numprocs, i;
  MPI_Status status;

  MPI_Comm_size(comm,&numprocs);
  MPI_Comm_rank(comm,&rank);

  if (rank != 0) MPI_Recv(buffer,count,datatype,MPI_ANY_SOURCE,MPI_ANY_TAG,comm,&status);

  for (i = 0 ;; i++)
    if (rank < ipow(2,i)) {
      if (ipow(2,i) + rank >= numprocs) break;
      //printf("Se envia al proceso -> %i | el dato -> %i | FROM: %i\n", ipow(2,i)+rank,*(int *)buffer,rank);
      MPI_Send(buffer,count,datatype,ipow(2,i)+rank,0,comm);
    }
  //printf("P -> %i Sale",rank);
  return 0;
};


int main(int argc, char *argv[]){

  int i, j, prime, n = 2, count;
  int numprocs, rank, totalcount;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  while (1){

    // E/S gestionada por proceso 0

    if (rank == 0){
      printf("Enter the maximum number to check for primes: (0 quits) \n");
      scanf("%d",&n);
    }

    MPI_BinomialtreeBcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (n == 0) break;

    count = 0;

    //For every number lower than the N the user entered:
    for (i = 2+rank; i < n; i+=numprocs) {

      prime = 1;

      // Check if the number i is prime (dividing by every j lower than i)
      for (j = 2; j < i; j++)
        if((i%j) == 0) {
          prime = 0;
          break;
        }

      count += prime;
    }

    //Receives count from all other processes and adds them to a total count
    MPI_FlattreeReduce(&count, &totalcount, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0)
      printf("The number of primes lower than %d is %d\n", n, totalcount);

  }
  MPI_Finalize();
}
