#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

// Configurable block size for Block-Cyclic distribution
// 1024 chosen to fit length of cache line in modern CPU
#define BLOCK_SIZE 1024

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
    int rank, size, provided;

    // Initialize MPI environment for multithreaded OpenMP execution
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    long long n = 0;
    if (rank == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: mpirun -np <procs> %s <N>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return 1;
        }
        n = atoll(argv[1]);
    }

    // Broadcast value of 'n' to all processes
    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (n < 3) {
        if (rank == 0) printf("No primes strictly less than %lld.\n", n);
        MPI_Finalize();
        return 0;
    }

    // Rank-level dynamic memory allocation
    long long local_capacity = 1024;
    long long *local_primes = (long long *)malloc(local_capacity * sizeof(long long));
    if (!local_primes) {
        fprintf(stderr, "[Rank %d] Error allocating local prime memory.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        return 1;
    }
    long long local_count = 0;

    // Base primes 2 and 3 stored on Rank 0
    if (rank == 0) {
        if (n > 2) local_primes[local_count++] = 2;
        if (n > 3) local_primes[local_count++] = 3;
    }

    // Upper bound on 6k +/- 1 candidate indices strictly below N
    long long max_candidate_index = (n / 3) * 2 + 2;
    long long total_blocks = (max_candidate_index + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Synchronize processes and begin overall timer
    MPI_Barrier(MPI_COMM_WORLD);
    double start_parallel_time = MPI_Wtime();
    double comp_start = MPI_Wtime();

    int total_omp_threads = 0;
    double *thread_exec_times = NULL;

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int num_threads = omp_get_num_threads();

        #pragma omp single
        {
            total_omp_threads = num_threads;
            thread_exec_times = (double *)malloc(num_threads * sizeof(double));
        }

        double thread_start = omp_get_wtime();

        long long t_capacity = 256;
        long long *t_primes = (long long *)malloc(t_capacity * sizeof(long long));
        long long t_count = 0;

        // BLOCK-CYCLIC MPI DISTRIBUTION:
        // Outer loop cycles through block IDs assigned to this rank (block += size)
        // Inner loop iterates over contiguous BLOCK_SIZE candidates
        #pragma omp for schedule(dynamic, 1) nowait
        for (long long block = rank; block < total_blocks; block += size) {
            long long start_i = block * BLOCK_SIZE;
            long long end_i = start_i + BLOCK_SIZE;
            if (end_i > max_candidate_index) end_i = max_candidate_index;

            for (long long i = start_i; i < end_i; i++) {
                // Generates 6k+5 and 6k+7 candidates
                long long val = 6 * (i / 2) + 5 + (i % 2) * 2;

                if (val >= n) continue;

                if (is_prime(val)) {
                    if (t_count >= t_capacity) {
                        t_capacity *= 2;
                        long long *temp = (long long *)realloc(t_primes, t_capacity * sizeof(long long));
                        t_primes = temp;
                    }
                    t_primes[t_count++] = val;
                }
            }
        }

        // Merge thread-local primes into rank-level array safely
        #pragma omp critical
        {
            while (local_count + t_count > local_capacity) {
                local_capacity *= 2;
                long long *temp = (long long *)realloc(local_primes, local_capacity * sizeof(long long));
                local_primes = temp;
            }
            for (long long j = 0; j < t_count; j++) {
                local_primes[local_count++] = t_primes[j];
            }
        }

        double thread_end = omp_get_wtime();
        if (thread_exec_times) {
            thread_exec_times[tid] = thread_end - thread_start;
        }

        free(t_primes);
    }

    double comp_end = MPI_Wtime();
    double rank_comp_time = comp_end - comp_start;

    // Output thread timings sequentially rank-by-rank
    for (int r = 0; r < size; r++) {
        if (rank == r) {
            printf("--------------------------------------------\n");
            printf("Current MPI Rank %2d\n", rank);
            printf("--------------------------------------------\n");
            if (thread_exec_times) {
                for (int t = 0; t < total_omp_threads; t++) {
                    printf("Thread %2d | Completion Time: %.6f seconds\n", t, thread_exec_times[t]);
                }
            }
            printf("\n");
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    free(thread_exec_times);

    // Gather individual process compute times to Rank 0
    //========================================================================================
    // THIS CHUNK OF CODE IS COMPLETED WITH THE HELP OF CHATGPT (FREE NON-LOGGED IN VERSION)
    double *all_comp_times = NULL;
    if (rank == 0) {
        all_comp_times = (double *)malloc(size * sizeof(double));
    }
    MPI_Gather(&rank_comp_time, 1, MPI_DOUBLE, all_comp_times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Workload Balance Reductions
    double max_comp_time, min_comp_time, sum_comp_time;
    MPI_Reduce(&rank_comp_time, &max_comp_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&rank_comp_time, &min_comp_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&rank_comp_time, &sum_comp_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    int local_count_int = (int)local_count;

    int *recvcounts = NULL;
    int *displs = NULL;

    if (rank == 0) {
        recvcounts = (int *)malloc(size * sizeof(int));
    }

    MPI_Gather(&local_count_int, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    //========================================================================================

    long long *all_primes = NULL;
    long long total_primes = 0;

    if (rank == 0) {
        displs = (int *)malloc(size * sizeof(int));
        displs[0] = 0;
        total_primes = recvcounts[0];

        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + recvcounts[i - 1];
            total_primes += recvcounts[i];
        }

        all_primes = (long long *)malloc(total_primes * sizeof(long long));
    }

    MPI_Gatherv(local_primes, local_count_int, MPI_LONG_LONG, all_primes, recvcounts, displs, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    // Sort prime results globally on Rank 0
    if (rank == 0) {
        qsort(all_primes, total_primes, sizeof(long long), compare_long_long);
    }

    double end_parallel_time = MPI_Wtime();
    double parallel_duration = end_parallel_time - start_parallel_time;

    // File Writing & Metric Reporting on Rank 0
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
