#include <mpi.h>
#include <stdio.h>
#include <faasm/faasm.h>
#include <faasm/print.h>
#include <string.h>


FAASM_MAIN_FUNC() {
    MPI_Init(NULL, NULL);

    int rank;
    int worldSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

    // Create an array of three numbers for this process
    int numsThisProc[3] = {rank, 10 * rank, 100 * rank};
    int *expected = new int[3];
    int *result = new int[3];

    // Build expectation
    memset(expected, 0, 3*sizeof(int));
    for (int r = 0; r < worldSize; r++) {
        expected[0] += r;
        expected[1] += 10 * r;
        expected[2] += 100 * r;
    }

    // Call allreduce
    MPI_Allreduce(numsThisProc, result, 3, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    // Check vs. expectation
    if (result[0] != expected[0] || result[1] != expected[1] || result[2] != expected[2]) {
        printf("Rank %i - Allreduce failed: Expected ", rank);
        faasm::printArray(expected, 3);
        printf(" Actual ");
        faasm::printArray(result, 3);
        printf("\n");
    } else {
        printf("Rank %i - Allreduce as expected\n", rank);
    }

    MPI_Finalize();

    return MPI_SUCCESS;
}