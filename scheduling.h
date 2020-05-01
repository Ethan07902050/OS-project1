#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "process.h"
#define FIFO 1
#define RR 2
#define SJF 3
#define PSJF 4

int next_process(struct Process proc[], int N, int policy, int running);
void scheduling(struct Process proc[], int N, int policy);
void rr_scheduling(struct Process proc[], int N);

#endif