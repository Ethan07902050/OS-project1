#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include "scheduling.h"
#include "process.h"

int to_id(char policy[]) {
    if(strcmp(policy, "FIFO") == 0) return FIFO;
    if(strcmp(policy, "RR") == 0) return RR;
    if(strcmp(policy, "SJF") == 0) return SJF;
    if(strcmp(policy, "PSJF") == 0) return PSJF;
}

sem_t* init_sem(int i) {
    char sen_name[10];
    sprintf(sen_name, "/sem%d", i);
    return sem_open(sen_name, O_CREAT, 0644, 0);
}

int destroy_sem(int i) {
    char sem_name[10];
    sprintf(sem_name, "/sem%d", i);
    sem_unlink(sem_name);
}

int main() {
    int N;
    char policy[5];
    scanf("%s", policy);
    scanf("%d", &N);
    struct Process *proc = (struct Process*)malloc(sizeof(struct Process) * N);
    sem = (sem_t**)malloc(sizeof(sem_t*) * N);
    for(int i = 0; i < N; i++) {
        scanf("%s%d%d\n", proc[i].name, &proc[i].ready, &proc[i].exec);
        proc[i].pid = -1;
        proc[i].id = i;
        sem[i] = init_sem(i);
    }

    int policy_id = to_id(policy);
    if(policy_id == RR) rr_scheduling(proc, N);
    else scheduling(proc, N, policy_id);

    for(int i = 0; i < N; i++) destroy_sem(i);
}