// Performs a serial prime number search for numbers strictly less than n
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// Function prototype
int TestForPrime(int inNum);

int main(int argc, char *argv[]) {
    int inN = 0;
    int k;
    int prime_count = 0;
    int terminal_printed = 0;
    struct timespec start, end;
    double time_taken;
    FILE *pFile = NULL;

    // 1. Handle Input: Support both command-line arguments and interactive terminal input
    if (argc > 1) {
        inN = atoi(argv[1]);
    } else {
        printf("Enter the value of n: ");
        if (scanf("%d", &inN) != 1) {
            fprintf(stderr, "Error: Invalid input. Please enter a valid integer.\n");
            return 1;
        }
    }

    // Input validation
    if (inN <= 0) {
        fprintf(stderr, "Error: n must be a positive integer greater than 0.\n");
        return 1;
    }

    printf("==================================================\n");
    printf("Serial Prime Finder\n");
    printf("Searching for primes strictly less than n = %d\n", inN);
    printf("==================================================\n");

    // 2. High-Resolution Timing Start (Wall-clock time)
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("Failed to get starting time");
        return 1;
    }

    // Determine output destination based on specification thresholds
    if (inN > 100) {
        pFile = fopen("primes.txt", "w");
        if (pFile == NULL) {
            perror("Error opening primes.txt for writing");
            return 1;
        }
        printf("Primes strictly less than 100: ");
    } else {
        printf("Primes strictly less than %d: ", inN);
    }

    // 3. Prime Search Logic
    // 2 is the first and only even prime number
    if (inN > 2) {
        prime_count++;
        printf("2");
        terminal_printed = 1;
    }

    // Check odd numbers starting from 3 up to inN-1 (strictly less than inN)
    for (k = 3; k < inN; k += 2) {
        if (TestForPrime(k)) {
            prime_count++;
            if (k < 100) {
                if (terminal_printed) {
                    printf(", %d", k);
                } else {
                    printf("%d", k);
                    terminal_printed = 1;
                }
            } else {
                // If k >= 100, we write to file (which is only open if inN > 100)
                if (pFile != NULL) {
                    fprintf(pFile, "%d\n", k);
                }
            }
        }
    }

    // Finalise output formatting
    printf("\n");
    if (pFile != NULL) {
        fclose(pFile);
        printf("Results strictly >= 100 successfully written to: primes.txt\n");
    }

    // 4. High-Resolution Timing End
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("Failed to get ending time");
        return 1;
    }

    // Calculate elapsed time in seconds
    time_taken = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("--------------------------------------------------\n");
    printf("Total Primes Found      : %d\n", prime_count);
    printf("Total Execution Time    : %f seconds\n", time_taken);
    printf("==================================================\n");

    return 0;
}

// Function definition
int TestForPrime(int inNum) {
    int divisor;
    int limit;

    // Numbers <= 1 are not prime
    if (inNum <= 1) return 0;
    
    // 2 is prime
    if (inNum == 2) return 1;
    
    // Any other even number is not prime
    if (inNum % 2 == 0) return 0;

    // Check divisibility by odd numbers up to sqrt(inNum)
    limit = (int)sqrt(inNum);
    for (divisor = 3; divisor <= limit; divisor += 2) {
        if (inNum % divisor == 0) {
            return 0; // Found a factor, not prime
        }
    }
    
    return 1; // No factors found, is prime
}
