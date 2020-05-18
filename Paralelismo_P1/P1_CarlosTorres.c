//Autores:
//  Carlos Torres Paz - 54225269D
//  Daniel Sergio Vega Rodríguez - 34277051J

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char *argv[]){

  int i, j, k, prime, n, count;
  int numprocs, rank, recbuff;

  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  while (1){

    // E/S gestionada por proceso 0
    if (rank == 0){
      printf("Enter the maximum number to check for primes: (0 quits) \n");
      scanf("%d",&n);

      for (k = 1; k<numprocs; k++){
        MPI_Send(&n, 1, MPI_INT, k, 0, MPI_COMM_WORLD);
      }

    }else{
      MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }

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


    // E/S gestionada por proceso 0
    if (rank == 0){
      //Receives count from all other processes and adds them to its own count
      for (k=1; k<numprocs; k++){
        MPI_Recv(&recbuff, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        count+=recbuff;
      }

      printf("The number of primes lower than %d is %d\n", n, count);
    }
    else{
      MPI_Send(&count, 1, MPI_INT, 0, rank, MPI_COMM_WORLD);
    }

  }

  MPI_Finalize();
}
