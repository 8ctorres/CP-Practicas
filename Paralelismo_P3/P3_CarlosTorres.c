#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>


/*
  Carlos Torres - 54225269D
  Daniel Sergio Vega - 34277051J
 */


#define DEBUG 0
#define ROOT_P 0
#define SENDS_T_PROC 22
#define SENDS_T_COMM 33

/* Translation of the DNA bases
   A -> 0
   C -> 1
   G -> 2
   T -> 3
   N -> 4*/


#define M  1000 // Number of sequences
#define N  200000  // Number of bases per sequence

// The distance between two bases
int base_distance(int base1, int base2){

  if((base1 == 4) || (base2 == 4)){
    return 3;
  }

  if(base1 == base2) {
    return 0;
  }

  if((base1 == 0) && (base2 == 3)) {
    return 1;
  }

  if((base2 == 0) && (base1 == 3)) {
    return 1;
  }

  if((base1 == 1) && (base2 == 2)) {
    return 1;
  }

  if((base2 == 2) && (base1 == 1)) {
    return 1;
  }

  return 2;
}

int main(int argc, char *argv[]) {

  int i, j, rank, numprocs, block_size;
  int *data1, *data2, *part_data1, *part_data2;
  int *result, *part_result;
  MPI_Status mpi_status;
  struct timeval tv1, tv2, tv3, tv4;

  MPI_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);

  //Calculates block size
  block_size = M/numprocs + (M % numprocs ? 1 : 0);

  if(rank==ROOT_P){
    printf("Num procs: %d, Matrix size %d x %d\n", numprocs, M, N);
    //Initialization done by process root (0)
    //Creates the matrixes and adds the padding if needed
    data1 = (int *) malloc(block_size*numprocs*N*sizeof(int));
    data2 = (int *) malloc(block_size*numprocs*N*sizeof(int));
    result = (int *) malloc(block_size*numprocs*sizeof(int));

    //Initialize matrixes
    for(i=0;i<M;i++) {
      for(j=0;j<N;j++) {
        data1[i*N+j] = (i+j)%5;
        data2[i*N+j] = ((i-j)*(i-j))%5;
      }
    }
  }

  part_data1 = (int *) malloc(block_size*N*sizeof(int));
  part_data2 = (int *) malloc(block_size*N*sizeof(int));
  part_result = (int *) malloc(block_size*sizeof(int));

  gettimeofday(&tv1, NULL);

  //Scatter both matrixes
  MPI_Scatter(data1, block_size*N, MPI_INT, part_data1, block_size*N, MPI_INT, ROOT_P, MPI_COMM_WORLD);
  MPI_Scatter(data2, block_size*N, MPI_INT, part_data2, block_size*N, MPI_INT, ROOT_P, MPI_COMM_WORLD);

  gettimeofday(&tv2, NULL);

  int local_block_size;

  //Last process recalculates block_size, all other use the general block size
  if (rank == numprocs-1)
    local_block_size = M - block_size*(numprocs-1);
  else
    local_block_size = block_size;

  //Each process calculates its own part of the data
  for(i=0; i < local_block_size; i++) {
    part_result[i]=0;
    for(j=0;j<N;j++) {
      part_result[i] += base_distance(part_data1[i*N+j], part_data2[i*N+j]);
    }
  }

  gettimeofday(&tv3, NULL);

  //Gather result
  MPI_Gather(part_result, block_size, MPI_INT, result, block_size, MPI_INT, ROOT_P, MPI_COMM_WORLD);

  gettimeofday(&tv4, NULL);

  int t_comm = ((tv2.tv_usec - tv1.tv_usec)+ 1000000 * (tv2.tv_sec - tv1.tv_sec)) + ((tv4.tv_usec - tv3.tv_usec) + 1000000*(tv4.tv_sec - tv3.tv_sec));
  int t_proc = (tv3.tv_usec - tv2.tv_usec)+ 1000000 * (tv3.tv_sec - tv2.tv_sec);

  /*Display result */
  if ((DEBUG) && (rank == ROOT_P)){
    for(int x=0;x<numprocs;x++) {
      printf("Proceso %d: ", x);
      for (int y=0; y<block_size; y++){
        if (x*block_size+y < M)
          printf(" %d \t ",result[x*block_size+y]);
      }
      printf("\n");
    }
    printf("\n");
  }

  //Prints times for every process
  if (rank == ROOT_P){
    printf ("Process %d, Communications time (seconds)  = %lf\n", rank, (double) t_comm/1E6);
    printf ("Process %d, Data processing time (seconds) = %lf\n", rank, (double) t_proc/1E6);
    for(int n=1; n<numprocs; n++){
      MPI_Recv(&t_comm, 1, MPI_INT, n, SENDS_T_COMM, MPI_COMM_WORLD, &mpi_status);
      printf ("Process %d, Communications time (seconds)  = %lf\n", n, (double) t_comm/1E6);
      MPI_Recv(&t_proc, 1, MPI_INT, n, SENDS_T_PROC, MPI_COMM_WORLD, &mpi_status);
      printf ("Process %d, Data processing time (seconds) = %lf\n", n, (double) t_proc/1E6);
    }
  }else{
    MPI_Send(&t_comm, 1, MPI_INT, ROOT_P, SENDS_T_COMM, MPI_COMM_WORLD);
    MPI_Send(&t_proc, 1, MPI_INT, ROOT_P, SENDS_T_PROC, MPI_COMM_WORLD);
  }

  if (rank == ROOT_P) {
    free(data1);
    free(data2);
    free(result);
  }

  free(part_data1);
  free(part_data2);
  free(part_result);

  MPI_Finalize();

  return 0;
}
