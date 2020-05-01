#ifndef PROCESS_H
#define PROCESS_H
#define PARENT_CPU 0
#define CHILD_CPU 1
#define GETTIME 439
#define PRINTK 440

struct Process {
    char name[32];
    int ready, exec;
    int id;
    pid_t pid;
};
sem_t **sem;

void unit_time();
void assign_cpu(pid_t pid, int core);
pid_t create(struct Process *p);
void block(pid_t pid);
void wakeup(pid_t pid);

#endif