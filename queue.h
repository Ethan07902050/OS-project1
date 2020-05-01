#ifndef QUEUE_H_
#define QUEUE_H_
#define CAPACITY 100

struct Queue {
    int queue[CAPACITY];
    int front, back;
};

void init(struct Queue *q);
void print_queue(struct Queue *q);
int is_empty(struct Queue *q);
int is_full(struct Queue *q);
int pop(struct Queue *q);
int push(struct Queue *q, int x);

#endif