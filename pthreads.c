// ------------------------------------------------------------------------- //
// Performs a multithreaded prime number search using POSIX threads
// //////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

// Struct representing thread-local data and output storage
typedef struct {
    int thread_id;          // Unique identifier (0 to num_threads - 1)
    int num_threads;        // Total number of threads spawned
    int inN;                // Upper search limit (exclusive)
    int *pLocalPrimes;      // Pointer to thread-local array of found primes
    int local_count;        // Number of primes found by this thread
    int local_capacity;     // Capacity of the thread-local array
} ThreadData;

// Function prototypes
int TestForPrime(int inNum);
void AddPrime(ThreadData *pData, int inPrime);
void *FindPrimesThread(void *pArg);

int main(int argc, char *argv[]) {
    int inN = 0;
    int inNumThreads = 0;
    int i;
    int prime_count;
    int primes_written;
    int *pIndices = NULL;
    double time_taken;
    FILE *pFile = NULL;
    pthread_t *hThread = NULL;
    ThreadData *pThreadData = NULL;
    struct timespec start, end;

    // 1. Input parsing: Supports CLI arguments for automation or interactive prompt
    if (argc >= 3) {
        inN = atoi(argv[1]);
        inNumThreads = atoi(argv[2]);
    } else {
        printf("Enter the value of n: ");
        if (scanf("%d", &inN) != 1) {
            fprintf(stderr, "Error: Invalid input for n.\n");
            return 1;
        }
        printf("Enter the number of threads: ");
        if (scanf("%d", &inNumThreads) != 1) {
            fprintf(stderr, "Error: Invalid input for thread count.\n");
            return 1;
        }
    }

    if (inN < 0) {
        fprintf(stderr, "Error: n must be a non-negative integer.\n");
        return 1;
    }
    if (inNumThreads < 1) {
        fprintf(stderr, "Error: Thread count must be at least 1.\n");
        return 1;
    }

    printf("==================================================\n");
    printf("POSIX Threads Prime Finder\n");
    printf("Searching for primes strictly less than n = %d\n", inN);
    printf("Number of threads: %d\n", inNumThreads);
    printf("Workload scheme  : Cyclic Interleaving (Odd Numbers)\n");
    printf("==================================================\n");

    // Start measuring high-precision monotonic clock time
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("Failed to get starting time");
        return 1;
    }

    // 2. Thread creation and data allocation
    hThread = (pthread_t *)malloc(inNumThreads * sizeof(pthread_t));
    pThreadData = (ThreadData *)malloc(inNumThreads * sizeof(ThreadData));
    if (hThread == NULL || pThreadData == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for thread structures.\n");
        return 1;
    }

    for (i = 0; i < inNumThreads; i++) {
        pThreadData[i].thread_id = i;
        pThreadData[i].num_threads = inNumThreads;
        pThreadData[i].inN = inN;
        pThreadData[i].pLocalPrimes = NULL;
        pThreadData[i].local_count = 0;
        pThreadData[i].local_capacity = 0;

        if (pthread_create(&hThread[i], NULL, FindPrimesThread, &pThreadData[i]) != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            return 1;
        }
    }

    // 3. Thread synchronization: Wait for all threads to finish
    for (i = 0; i < inNumThreads; i++) {
        pthread_join(hThread[i], NULL);
    }

    // 4. Multi-way merge setup
    prime_count = (inN > 2) ? 1 : 0; // Account for the only even prime '2'
    for (i = 0; i < inNumThreads; i++) {
        prime_count += pThreadData[i].local_count;
    }

    pIndices = (int *)calloc(inNumThreads, sizeof(int));
    if (pIndices == NULL) {
        fprintf(stderr, "Error: Failed to allocate merge indices.\n");
        return 1;
    }

    // Determine output destination based on specification thresholds
    if (inN > 100) {
        pFile = fopen("primes.txt", "w");
        if (pFile == NULL) {
            perror("Error opening primes.txt for writing");
            // Free allocated memory before exit
            for (i = 0; i < inNumThreads; i++) {
                if (pThreadData[i].pLocalPrimes != NULL) {
                    free(pThreadData[i].pLocalPrimes);
                }
            }
            free(hThread);
            free(pThreadData);
            free(pIndices);
            return 1;
        }
    } else {
        printf("Primes strictly less than %d: ", inN);
    }

    // Write the first prime '2' if search range allows it
    if (inN > 2) {
        if (inN <= 100) {
            printf("2");
        } else {
            fprintf(pFile, "2\n");
        }
    }

    // Merge and print remaining primes in strictly sorted ascending order
    primes_written = (inN > 2) ? 1 : 0;
    while (primes_written < prime_count) {
        int min_val = -1;
        int min_thread_idx = -1;

        // Multi-way selection among the current heads of the pre-sorted thread arrays
        for (i = 0; i < inNumThreads; i++) {
            if (pIndices[i] < pThreadData[i].local_count) {
                int val = pThreadData[i].pLocalPrimes[pIndices[i]];
                if (min_val == -1 || val < min_val) {
                    min_val = val;
                    min_thread_idx = i;
                }
            }
        }

        if (min_thread_idx != -1) {
            if (inN <= 100) {
                printf(", %d", min_val); // Simple terminal print
            } else {
                fprintf(pFile, "%d\n", min_val); // Pure file print
            }
            pIndices[min_thread_idx]++;
            primes_written++;
        }
    }

    if (inN <= 100) {
        printf("\n");
    } else if (pFile != NULL) {
        fclose(pFile);
        printf("Results successfully written to: primes.txt\n");
    }

    // Stop measuring clock time
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("Failed to get ending time");
        return 1;
    }

    time_taken = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("--------------------------------------------------\n");
    printf("Total Primes Found      : %d\n", prime_count);
    printf("Total Execution Time    : %f seconds\n", time_taken);
    printf("================================================--\n");

    // 5. Clean up allocations
    for (i = 0; i < inNumThreads; i++) {
        if (pThreadData[i].pLocalPrimes != NULL) {
            free(pThreadData[i].pLocalPrimes);
        }
    }
    free(hThread);
    free(pThreadData);
    free(pIndices);

    return 0;
}

/**
 * TestForPrime - Checks if a number inNum is prime.
 * Optimized by checking divisibility only up to sqrt(inNum).
 */
int TestForPrime(int inNum) {
    int i;
    int limit;

    if (inNum < 2) return 0;
    if (inNum == 2) return 1;
    if (inNum % 2 == 0) return 0;
    
    limit = (int)sqrt(inNum);
    for (i = 3; i <= limit; i += 2) {
        if (inNum % i == 0) return 0;
    }
    return 1;
}

/**
 * AddPrime - Dynamically appends a prime to a thread's local array.
 */
void AddPrime(ThreadData *pData, int inPrime) {
    if (pData->local_count >= pData->local_capacity) {
        pData->local_capacity = (pData->local_capacity == 0) ? 1024 : pData->local_capacity * 2;
        int *temp = (int *)realloc(pData->pLocalPrimes, pData->local_capacity * sizeof(int));
        if (temp == NULL) {
            fprintf(stderr, "Error: Thread %d memory allocation failed!\n", pData->thread_id);
            exit(EXIT_FAILURE);
        }
        pData->pLocalPrimes = temp;
    }
    pData->pLocalPrimes[pData->local_count++] = inPrime;
}

/**
 * FindPrimesThread - Thread entry point function.
 * Implements cyclic interleaving over odd numbers.
 */
void *FindPrimesThread(void *pArg) {
    ThreadData *pData = (ThreadData *)pArg;
    int id = pData->thread_id;
    int T = pData->num_threads;
    int n = pData->inN;
    long long start_val = 3 + 2LL * id;
    long long step_size = 2LL * T;
    long long k;

    for (k = start_val; k < n; k += step_size) {
        if (TestForPrime((int)k)) {
            AddPrime(pData, (int)k);
        }
    }
    return NULL;
}
