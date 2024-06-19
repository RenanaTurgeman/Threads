#include <stdio.h>
#include <stdbool.h>
#include <time.h> 

#include "queue.c"


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

// Function for the producerThread
void *inputNumbers(void *arg) {
    Queue *q = (Queue *)arg;
    int num;

    while (1) {
        if (scanf("%d", &num) == EOF) {
            break; // Exit the loop if end of input is reached
        }
        pushToQueue(q, num);
    }

    // Push sentinel values to signal end of input
    for (int i = 0; i < 6; ++i) {
        pushToQueue(q, -1);
    }

    return NULL;
}

// Function for the consumerThreads
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
            atomic_fetch_add(&q->prime_count, 1);
            // printf("%d is prime\n", num);
        }
    }
    return NULL;
}

int main() {
        Queue *queue = createQueue();

    pthread_t producerThread;
    pthread_t consumerThreads[6];

    pthread_create(&producerThread, NULL, inputNumbers, queue);

    for (int i = 0; i < 6; ++i) {
        pthread_create(&consumerThreads[i], NULL, outputNumbers, queue);
    }

    pthread_join(producerThread, NULL);

    for (int i = 0; i < 6; ++i) {
        pthread_join(consumerThreads[i], NULL);
    }

    printf("%d total primes.\n", queue->prime_count);

    removeQueue(queue);

    return 0;
}
