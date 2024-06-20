#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include "lfq.h"

// Function to check if a number is prime
// bool isPrime(int n) {
//     if (n <= 1) {
//         return false;
//     }
//     for (int i = 2; i * i <= n; i++) {
//         if (n % i == 0) {
//             return false;
//         }
//     }
//     return true;
// }

// Function to check if a number is prime
bool isPrime(int n) {
    if (n <= 1) {
        return false;
    }
    if (n == 2 || n == 3) {
        return true;
    }
    if (n % 2 == 0 || n % 3 == 0) {
        return false;
    }

    // Check divisors up to the square root of n
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }

    return true;
}


// Function for the producer thread to input numbers into the queue
void *inputNumbers(void *arg) {
    struct lfq_ctx *ctx = (struct lfq_ctx *)arg;
    int num;

    while (scanf("%d", &num) != EOF) {
        lfq_enqueue(ctx, (void *)(long)num); // Push the number into the queue
    }

    // Push sentinel values to signal end of input
    for (int i = 0; i < 6; ++i) {
        lfq_enqueue(ctx, (void *)(long)-1); // Sentinel value (-1) to indicate end of input for each consumer thread
    }

    return NULL;
}

// Function for the consumer threads to process numbers from the queue
void *outputNumbers(void *arg) {
    struct lfq_ctx *ctx = (struct lfq_ctx *)arg;
    int num;
    int local_prime_count = 0;

    while (1) {
        num = (int)(long)lfq_dequeue(ctx);
        if (num == -1) {
            lfq_enqueue(ctx, (void *)(long)-1);  // Signal to other consumers
            break;
        }
        if (isPrime(num)) {
            local_prime_count++;
        }
    }

    return (void *)(long)local_prime_count;
}

int main() {
    struct lfq_ctx queue;
    lfq_init(&queue, 6); // Initialize the queue with space for 6 consumer threads

    pthread_t producerThread;
    pthread_t consumerThreads[6];

    // Create the producer thread
    pthread_create(&producerThread, NULL, inputNumbers, &queue);

    // Create the consumer threads
    for (int i = 0; i < 6; ++i) {
        pthread_create(&consumerThreads[i], NULL, outputNumbers, &queue);
    }

    // Wait for the producer thread to finish
    pthread_join(producerThread, NULL);

    // Wait for all consumer threads to finish
    int total_primes = 0;
    void *ret_val;
    for (int i = 0; i < 6; ++i) {
        pthread_join(consumerThreads[i], &ret_val);
        total_primes += (int)(long)ret_val;
    }

    printf("%d total primes.\n", total_primes);

    lfq_clean(&queue);

    return 0;
}
