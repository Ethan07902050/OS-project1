#include <stdio.h>
#include "queue.h"

void init(struct Queue *q) {
    for(int i = 0; i < CAPACITY; i++) q->queue[i] = 0;
    q->front = q->back = 0;
}

void print_queue(struct Queue *q) {
    printf("Queue:");
    for(int i = q->front; i != q->back; i = (i + 1) % CAPACITY) {
        printf(" %d", q->queue[i]);
    }
    printf("\n");
}

int is_empty(struct Queue *q) {
    return q->front == q->back;
}

int is_full(struct Queue *q) {
    return (q->back + 1) % CAPACITY == q->front;
}

int pop(struct Queue *q) {
    if(!is_empty(q)) {
        int ret = q->queue[q->front];
        q->front = (q->front + 1) % CAPACITY;
        return ret;
    }
    return -1;
}

int push(struct Queue *q, int x) {
    if(!is_full(q)) {
        q->queue[q->back] = x;
        q->back = (q->back + 1) % CAPACITY;
        return 0;
    }
    return -1;
}