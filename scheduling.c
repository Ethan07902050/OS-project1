#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <semaphore.h>
#include "process.h"
#include "scheduling.h"
#include "queue.h"

int counter = 0;

int next_process(struct Process proc[], int N, int policy, int running) {
    
    if(running != -1 && (policy == FIFO || policy == SJF))
        return running;

    int ret = -1;

    if(policy == FIFO) {
        for(int i = 0; i < N; i++) {
            if(proc[i].pid == -1 || proc[i].exec == 0)
                continue;
            if(ret == -1 || proc[i].ready < proc[ret].ready)
                ret = i;
        }
    }
    else if(policy == SJF || policy == PSJF) {
        for(int i = 0; i < N; i++) {
            if(proc[i].pid == -1 || proc[i].exec == 0)
                continue;
            if(ret == -1 || proc[i].exec < proc[ret].exec)
                ret = i;
        }
    }
    else {
        perror("unknown scheduling policy");
        _exit(0);
    }

    return ret;
}

void scheduling(struct Process proc[], int N, int policy) {

    int finish_cnt = 0;
    int running = -1, next;

    assign_cpu(getpid(), PARENT_CPU);
    wakeup(0);
    while(1) {  
#ifdef DEBUG
        if(counter % 500 == 0) printf("time = %d\n", counter);
#endif 
        if(running != -1 && proc[running].exec == 0) {
            waitpid(proc[running].pid, NULL, 0);

#ifdef DEBUG
            printf("time = %d\n", counter);
            printf("process %s stops\n", proc[next].name);
#endif
            running = -1;
            finish_cnt++;
        }
        
        if(finish_cnt == N) break;

        for(int i = 0; i < N; i++) {
            if(counter == proc[i].ready) {
                pid_t pid = create(&proc[i]);
                proc[i].pid = pid;
                block(pid);            
            }
        }

        next = next_process(proc, N, policy, running);
        if(running != next) {
            
            if(running != -1 && proc[running].exec != 0) {
                block(proc[running].pid);
#ifdef DEBUG
                printf("time = %d\n", counter);
                printf("process %s stops\n", proc[running].name);
#endif
            }

            if(next != -1) {
#ifdef DEBUG
                printf("time = %d\n", counter);
                printf("process %s starts\n", proc[next].name);
#endif
                sem_post(sem[next]);
                wakeup(proc[next].pid);
            }

            running = next;
        }
        if(running != -1) proc[running].exec--;
        counter++;
        unit_time();
    }
}

void rr_scheduling(struct Process proc[], int N) {
    int last = 0;
    int finish_cnt = 0;
    int running = -1, next;
    struct Queue q;
    init(&q);
    assign_cpu(getpid(), PARENT_CPU);
    wakeup(0);

    while(1) {   
#ifdef DEBUG
        if(counter % 500 == 0) printf("time = %d\n", counter);
#endif
        if(running != -1 && proc[running].exec == 0) {
            waitpid(proc[running].pid, NULL, 0);

#ifdef DEBUG
            printf("time = %d\n", counter);
            printf("process %s stops\n", proc[next].name);
#endif
            running = -1;
            finish_cnt++;
        }
        
        if(finish_cnt == N) break;

        for(int i = 0; i < N; i++) {
            if(counter == proc[i].ready) {
                pid_t pid = create(&proc[i]);
                proc[i].pid = pid;
                block(pid);
                push(&q, i);
            }
        }

        if(running != -1 && (counter - last) % 500 == 0 && !is_empty(&q)) {
#ifdef DEBUG
            printf("time = %d\n", counter);
            printf("process %s stops\n", proc[running].name);
#endif           
            block(proc[running].pid);
            push(&q, running);
            last = counter;
            running = -1;
        }

        if(running == -1) {
            next = pop(&q);
            if(next != -1) {
#ifdef DEBUG
                printf("time = %d\n", counter);
                printf("process %s starts\n", proc[next].name);
#endif
                wakeup(proc[next].pid);
                sem_post(sem[next]);
                running = next;
                last = counter;
            }
        }
        if(running != -1) proc[running].exec--;
        counter++;
        unit_time();
    }
}