#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "process.h"

void unit_time() {
    volatile unsigned long i;
    for(i = 0; i < 1000000UL; i++) ;
}

void assign_cpu(pid_t pid, int core) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);

    if(sched_setaffinity(pid, sizeof(set), &set) == -1) {
        perror("sched setaffinity");
        _exit(0);
    }
}

pid_t create(struct Process *p) {
    pid_t pid = fork();
    
    if(pid == 0) {
        long long int start_sec, stop_sec;
        long int start_nsec, stop_nsec;

        printf("%s %d\n", p->name, getpid());
        assign_cpu(getpid(), CHILD_CPU);
        sem_wait(sem[p->id]);

        syscall(GETTIME, &start_sec, &start_nsec);
        for(int i = 0; i < p->exec; i++) unit_time();
        syscall(GETTIME, &stop_sec, &stop_nsec);
        syscall(PRINTK, getpid(), start_sec, start_nsec, stop_sec, stop_nsec);

        _exit(0);
    }

    return pid;
}

void block(pid_t pid) {
    struct sched_param param;
    param.sched_priority = 0;
    if(sched_setscheduler(pid, SCHED_IDLE, &param) == -1) {
        perror("block");
        _exit(1);
    }
}

void wakeup(pid_t pid) {
    struct sched_param param;
    param.sched_priority = 0;
    if(sched_setscheduler(pid, SCHED_OTHER, &param) == -1) {
        perror("wakeup");
        _exit(1);
    }
}