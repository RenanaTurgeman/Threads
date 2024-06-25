#include <stdio.h>
#include <stdbool.h>
#include <time.h> 

#include "queue.c" // Header file containing the queue implementation


// Function to check if a number is prime
bool isPrime(int n) {
    if (n <= 1) {
        return false;
    }
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

// Function for the producer thread to input numbers into the queue
void *inputNumbers(void *arg) {
    Queue *q = (Queue *)arg;
    int num;

    while (1) {
        if (scanf("%d", &num) == EOF) {
            break; // Exit the loop if end of input is reached
        }
        pushToQueue(q, num); // Push the number into the queue
    }

    // Push sentinel values to signal end of input
    for (int i = 0; i < 6; ++i) {
        pushToQueue(q, -1); // Sentinel value (-1) to indicate end of input for each consumer thread
    }

    return NULL;
}

// Function for the consumer threads to process numbers from the queue
void *outputNumbers(void *arg) {
    Queue *q = (Queue *)arg;
    int num;

    while (1) {
        num = popFromQueue(q);
        if (num == -1) {
            pushToQueue(q, -1);  // Signal to other consumers
            break;
        }
        if (isPrime(num)) {
            atomic_fetch_add(&q->prime_count, 1); // Increment the prime count atomically
            // printf("%d is prime\n", num);
        }
    }
    return NULL;
}

int main() {
    Queue *queue = createQueue(); // Create a queue

    pthread_t producerThread;
    pthread_t consumerThreads[6];

    // Create the producer thread
    pthread_create(&producerThread, NULL, inputNumbers, queue);

    // Create the consumer threads
    for (int i = 0; i < 6; ++i) {
        pthread_create(&consumerThreads[i], NULL, outputNumbers, queue);
    }

    // Wait for the producer thread to finish
    pthread_join(producerThread, NULL);

    // Wait for all consumer threads to finish
    for (int i = 0; i < 6; ++i) {
        pthread_join(consumerThreads[i], NULL);
    }

    printf("%d total primes.\n", queue->prime_count);

    removeQueue(queue);

    return 0;
}