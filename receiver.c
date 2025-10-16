#include "receiver.h"

/* Local-only constants (match sender.c) */
static const char *SEM_EMPTY = "/lab1_sem_empty";
static const char *SEM_FULL  = "/lab1_sem_full";
static const int   MQ_PROJID = 0x51;
static const int   SHM_PROJID= 0x52;

static inline double delta_sec(struct timespec a, struct timespec b){
    return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec)/1e9;
}

static double g_recv_total = 0.0;
static int g_shmid_for_cleanup = -1;

static void init_mailbox_receiver(mailbox_t *mb, int mech){
    mb->flag = mech;
    if (mech == MSG_PASSING){
        key_t k = ftok(".", MQ_PROJID);
        if (k == -1){ perror("ftok mq"); exit(1); }
        mb->storage.msqid = msgget(k, IPC_CREAT | 0666);
        if (mb->storage.msqid == -1){ perror("msgget"); exit(1); }
    } else if (mech == SHARED_MEM){
        key_t k = ftok(".", SHM_PROJID);
        if (k == -1){ perror("ftok shm"); exit(1); }
        g_shmid_for_cleanup = shmget(k, MAX_MSG, IPC_CREAT | 0666);
        if (g_shmid_for_cleanup == -1){ perror("shmget"); exit(1); }
        mb->storage.shm_addr = (char*)shmat(g_shmid_for_cleanup, NULL, 0);
        if (mb->storage.shm_addr == (void*)-1){ perror("shmat"); exit(1); }
    } else {
        fprintf(stderr, "unknown mechanism: %d\n", mech);
        exit(1);
    }
}

static void close_mailbox_receiver(mailbox_t *mb){
    if (mb->flag == MSG_PASSING){
        msgctl(mb->storage.msqid, IPC_RMID, NULL);
    } else if (mb->flag == SHARED_MEM){
        shmdt(mb->storage.shm_addr);
        if (g_shmid_for_cleanup != -1) shmctl(g_shmid_for_cleanup, IPC_RMID, NULL);
        sem_unlink(SEM_EMPTY);
        sem_unlink(SEM_FULL);
    }
}

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    struct timespec t0, t1;

    if (mailbox_ptr->flag == MSG_PASSING){
        clock_gettime(CLOCK_MONOTONIC, &t0);
        ssize_t n = msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->msgText), 0, 0);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (n == -1){ perror("msgrcv"); exit(1); }
        g_recv_total += delta_sec(t1, t0);
    } else { // SHARED_MEM
        sem_t *sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, 1);
        sem_t *sem_full  = sem_open(SEM_FULL,  O_CREAT, 0666, 0);
        if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED){ perror("sem_open"); exit(1); }

        clock_gettime(CLOCK_MONOTONIC, &t0);
        sem_wait(sem_full);
        memset(message_ptr->msgText, 0, MAX_MSG);
        strncpy(message_ptr->msgText, mailbox_ptr->storage.shm_addr, MAX_MSG-1);
        message_ptr->mType = 1;
        sem_post(sem_empty);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        g_recv_total += delta_sec(t1, t0);

        sem_close(sem_empty);
        sem_close(sem_full);
    }
}

int main(int argc, char **argv){
    if (argc < 2){
        fprintf(stderr, "usage: %s <1|2>\n", argv[0]);
        return 1;
    }
    int mech = atoi(argv[1]);

    mailbox_t mb = {0};
    init_mailbox_receiver(&mb, mech);

    message_t msg;
    for(;;){
        receive(&msg, &mb);
        // 可依作業需求列印每筆接收資訊
        // printf("[receiver] %s\n", msg.msgText);
        if (strcmp(msg.msgText, EXIT_TAG) == 0) break;
    }

    printf("Total receiving time (mechanism=%d): %.6f sec\n", mech, g_recv_total);
    close_mailbox_receiver(&mb);
    return 0;
}
