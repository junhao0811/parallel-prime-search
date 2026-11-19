#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define CHUNK_SIZE 100

// Primality test using 6k +/- 1 trial division optimization
bool is_prime(long long n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (long long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

// Comparator function for qsort
int compare_long_long(const void *a, const void *b) {
    long long arg1 = *(const long long *)a;
    long long arg2 = *(const long long *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long long n = 0;
    if (rank == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <N>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return 1;
        }
        n = atoll(argv[1]);
    }

    // Step 1: Broadcast value of N to all processes···
    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // Initial dynamic allocation for local primes found
    long long local_capacity = 1024;
    long long *local_primes = (long long *)malloc(local_capacity * sizeof(long long));
    long long local_count = 0;

    long long total_chunks = (n - 2 + CHUNK_SIZE - 1) / CHUNK_SIZE;

    // Step 2: Synchronize and begin overall parallel wall-time measurement
    MPI_Barrier(MPI_COMM_WORLD);
    double start_parallel_time = MPI_Wtime();

    // Step 3: Measure pure computation time per rank
    double comp_start = MPI_Wtime();

    // Static Block-Cyclic Prime Search (Chunk size = 100)
    for (long long c = rank; c < total_chunks; c += size) {
        long long start_val = 2 + c * CHUNK_SIZE;
        long long end_val = start_val + CHUNK_SIZE;
        if (end_val > n) end_val = n;

        for (long long val = start_val; val < end_val; val++) {
            if (is_prime(val)) {
                if (local_count >= local_capacity) {
                    local_capacity *= 2;
                    local_primes = (long long *)realloc(local_primes, local_capacity * sizeof(long long));
                }
                local_primes[local_count++] = val;
            }
        }
    }

    double comp_end = MPI_Wtime();
    double rank_comp_time = comp_end - comp_start;

    // Step 4: Gather individual process compute times to Rank 0 for detailed reporting
    double *all_comp_times = NULL;
    if (rank == 0) {
        all_comp_times = (double *)malloc(size * sizeof(double));
    }
    MPI_Gather(&rank_comp_time, 1, MPI_DOUBLE, all_comp_times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Step 5: Workload Balance Reductions
    double max_comp_time, min_comp_time, sum_comp_time;
    MPI_Reduce(&rank_comp_time, &max_comp_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&rank_comp_time, &min_comp_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&rank_comp_time, &sum_comp_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Step 6: Collective Gather of Prime Counts & Arrays
    int *recvcounts = NULL;
    int *displs = NULL;
    int local_count_int = (int)local_count;

    if (rank == 0) {
        recvcounts = (int *)malloc(size * sizeof(int));
    }

    MPI_Gather(&local_count_int, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    long long *all_primes = NULL;
    long long total_primes = 0;

    if (rank == 0) {
        displs = (int *)malloc(size * sizeof(int));
        displs[0] = 0;
        total_primes = recvcounts[0];

        // Offset management
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + recvcounts[i - 1];
            total_primes += recvcounts[i];
        }

        all_primes = (long long *)malloc(total_primes * sizeof(long long));
    }

    // Collective gather of all found primes into Rank 0
    MPI_Gatherv(local_primes, local_count_int, MPI_LONG_LONG,
                all_primes, recvcounts, displs, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // Step 7: Sort collected primes to guarantee strict ascending order
    if (rank == 0) {
        qsort(all_primes, total_primes, sizeof(long long), compare_long_long);
    }

    // End parallel timing strictly before File I/O
    double end_parallel_time = MPI_Wtime();
    double parallel_duration = end_parallel_time - start_parallel_time;

    // Step 8: File Writing and Empirical Metrics Reporting
    if (rank == 0) {
        double io_start = MPI_Wtime();
        FILE *fp = fopen("primes_output.txt", "w");
        if (fp != NULL) {
            for (long long i = 0; i < total_primes; i++) {
                fprintf(fp, "%lld\n", all_primes[i]);
            }
            fclose(fp);
        } else {
            fprintf(stderr, "Error: Cannot open file for writing\n");
        }
        double io_end = MPI_Wtime();
        double io_duration = io_end - io_start;
        double total_program_time = parallel_duration + io_duration;

        double avg_comp_time = sum_comp_time / size;
        double load_imbalance = ((max_comp_time - min_comp_time) / max_comp_time) * 100.0;
        double comm_overhead = parallel_duration - max_comp_time;

        printf("==================== Execution Summary ====================\n");
        printf("Total Primes Found          : %lld\n", total_primes);
        printf("Parallel Wall Time (No I/O) : %.6f s\n", parallel_duration);
        printf("Communication/Gather Delay  : %.6f s\n", comm_overhead);
        printf("File I/O Duration           : %.6f s\n", io_duration);
        printf("Total Program Run Time      : %.6f s\n", total_program_time);
        printf("---------------- Per-Rank Computation ---------------------\n");
        for (int r = 0; r < size; r++) {
            printf("Rank %2d Computation Time       : %.6f s\n", r, all_comp_times[r]);
        }
        printf("---------------- Workload Balance Metrics -----------------\n");
        printf("Fastest Rank Computation    : %.6f s\n", min_comp_time);
        printf("Slowest Rank Computation    : %.6f s\n", max_comp_time);
        printf("Average Rank Computation    : %.6f s\n", avg_comp_time);
        printf("Workload Imbalance Spread   : %.2f%%\n", load_imbalance);
        printf("===========================================================\n");

        free(recvcounts);
        free(displs);
        free(all_primes);
        free(all_comp_times);
    }

    free(local_primes);
    MPI_Finalize();
    return 0;
}
