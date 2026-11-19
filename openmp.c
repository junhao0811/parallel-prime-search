// Performs a parallel prime number search using OpenMP

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <omp.h>

// Function prototype
int TestForPrime(int inNum);

int main(int argc, char *argv[]) {
    int inN = 0;
    int inNumThreads = 1;
    int i;
    int prime_count = 0;
    double time_taken;
    FILE *pFile = NULL;
    struct timespec start, end;

    // 1. Input parsing
    if (argc >= 3) {
        inN = atoi(argv[1]);
        inNumThreads = atoi(argv[2]);
    } else if (argc == 2) {
        inN = atoi(argv[1]);
        inNumThreads = omp_get_max_threads();
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

    // Input validation
    if (inN <= 0) {
        fprintf(stderr, "Error: n must be a positive integer.\n");
        return 1;
    }

    if (inNumThreads < 1) {
        fprintf(stderr, "Error: Thread count must be at least 1.\n");
        return 1;
    }

    printf("==================================================\n");
    printf("OpenMP Prime Finder\n");
    printf("Searching for primes strictly less than n = %d\n", inN);
    printf("Number of threads: %d\n", inNumThreads);
    printf("Workload scheme  : OpenMP static scheduling\n");
    printf("==================================================\n");

    // Start timer
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("Failed to get starting time");
        return 1;
    }

    // 2 is the only even prime
    if (inN > 2) {
        prime_count = 1;
    }

    /*
     * Parallel prime search.
     *
     * Each thread counts its own primes using a reduction.
     * The actual prime values are stored in a shared flag array so
     * that they can later be printed in ascending order.
     */
    char *prime_flags = calloc(inN, sizeof(char));

    if (prime_flags == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 1;
    }

    #pragma omp parallel for schedule(static, 1) reduction(+:prime_count) num_threads(inNumThreads)
    for (i = 3; i < inN; i += 2) {
        if (TestForPrime(i)) {
            prime_flags[i] = 1;
            prime_count++;
        }
    }

    // Determine output destination
    if (inN > 100) {
        pFile = fopen("primes.txt", "w");

        if (pFile == NULL) {
            perror("Error opening primes.txt for writing");
            free(prime_flags);
            return 1;
        }
    } else {
        printf("Primes strictly less than %d: ", inN);
    }

    // Print 2 separately
    if (inN > 2) {
        if (inN <= 100) {
            printf("2");
        } else {
            fprintf(pFile, "2\n");
        }
    }

    // Print remaining primes in ascending order
    for (i = 3; i < inN; i += 2) {
        if (prime_flags[i]) {
            if (inN <= 100) {
                printf(", %d", i);
            } else {
                fprintf(pFile, "%d\n", i);
            }
        }
    }

    // Finalise output
    if (inN <= 100) {
        printf("\n");
    } else {
        fclose(pFile);
        printf("Results successfully written to: primes.txt\n");
    }

    free(prime_flags);

    // Stop timer
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("Failed to get ending time");
        return 1;
    }

    // Calculate elapsed time
    time_taken = (end.tv_sec - start.tv_sec)
               + (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("--------------------------------------------------\n");
    printf("Total Primes Found      : %d\n", prime_count);
    printf("Total Execution Time    : %f seconds\n", time_taken);
    printf("==================================================\n");

    return 0;
}

// Tests whether a number is prime.

int TestForPrime(int inNum) {

    // Numbers below 2 are not prime
    if (inNum < 2)
        return 0;

    // 2 is prime
    if (inNum == 2)
        return 1;

    // Other even numbers are not prime
    if (inNum % 2 == 0)
        return 0;

    // Only test odd divisors up to sqrt(inNum)
    int limit = (int)sqrt(inNum);

    for (int divisor = 3; divisor <= limit; divisor += 2) {
        if (inNum % divisor == 0)
            return 0;
    }

    return 1;
}