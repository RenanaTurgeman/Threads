#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#define MAX_QUEUE_SIZE 1024 // Adjust according to your needs

typedef struct {
    int current_size;
    int max_capacity;
    int* buffer;
    int head;
    int tail;
    atomic_int prime_count;
    pthread_mutex_t lock;
    pthread_cond_t condition;
} Queue;

Queue* createQueue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    if (queue == NULL) {
        perror("Failed to create queue");
        exit(EXIT_FAILURE);
    }

    queue->buffer = (int*)malloc(MAX_QUEUE_SIZE * sizeof(int));
    if (queue->buffer == NULL) {
        perror("Failed to allocate queue buffer");
        free(queue);
        exit(EXIT_FAILURE);
    }

    queue->head = -1;
    queue->tail = -1;
    queue->current_size = 0;
    queue->max_capacity = MAX_QUEUE_SIZE;
    atomic_init(&queue->prime_count, 0);

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->condition, NULL);

    return queue;
}

int popFromQueue(Queue* queue) {
    pthread_mutex_lock(&queue->lock);

    while (queue->current_size == 0) {
        pthread_cond_wait(&queue->condition, &queue->lock); // Wait if the queue is empty
    }

    int value = queue->buffer[queue->head];
    if (queue->head == queue->tail) {
        queue->head = -1;
        queue->tail = -1;
    } else {
        queue->head = (queue->head + 1) % queue->max_capacity;
    }

    queue->current_size--;

    pthread_mutex_unlock(&queue->lock);
    pthread_cond_signal(&queue->condition); // Notify that a value has been removed

    return value;
}

void pushToQueue(Queue* queue, int value) {
    pthread_mutex_lock(&queue->lock);

    while ((queue->tail + 1) % queue->max_capacity == queue->head) {
        pthread_cond_wait(&queue->condition, &queue->lock); // Wait if the queue is full
    }

    if (queue->head == -1) {
        queue->head = 0;
        queue->tail = 0;
    } else {
        queue->tail = (queue->tail + 1) % queue->max_capacity;
    }

    queue->buffer[queue->tail] = value;
    queue->current_size++;

    pthread_mutex_unlock(&queue->lock);
    pthread_cond_signal(&queue->condition); // Notify that a value has been added
}


void removeQueue(Queue* queue) {
    free(queue->buffer);
    free(queue);
}