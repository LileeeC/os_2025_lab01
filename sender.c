#include "sender.h"

/* Local-only constants (both .c files must match) */
static const char *SEM_EMPTY = "/lab1_sem_empty";
static const char *SEM_FULL  = "/lab1_sem_full";
static const int   MQ_PROJID = 0x51;  // ftok project id for message queue
static const int   SHM_PROJID= 0x52;  // ftok project id for shared memory

static inline double delta_sec(struct timespec a, struct timespec b){
    return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec)/1e9;
}

static double g_send_total = 0.0;

/* Initialize mailbox for sender side */
static void init_mailbox_sender(mailbox_t *mb, int mech){
    mb->flag = mech;
    if (mech == MSG_PASSING){
        key_t k = ftok(".", MQ_PROJID);
        if (k == -1){ perror("ftok mq"); exit(1); }
        mb->storage.msqid = msgget(k, IPC_CREAT | 0666);
        if (mb->storage.msqid == -1){ perror("msgget"); exit(1); }
    } else if (mech == SHARED_MEM){
        key_t k = ftok(".", SHM_PROJID);
        if (k == -1){ perror("ftok shm"); exit(1); }
        int shmid = shmget(k, MAX_MSG, IPC_CREAT | 0666);
        if (shmid == -1){ perror("shmget"); exit(1); }
        mb->storage.shm_addr = (char*)shmat(shmid, NULL, 0);
        if (mb->storage.shm_addr == (void*)-1){ perror("shmat"); exit(1); }
        /* semaphores will be opened per send() call with named handles */
    } else {
        fprintf(stderr, "unknown mechanism: %d\n", mech);
        exit(1);
    }
}

static void close_mailbox_sender(mailbox_t *mb){
    if (mb->flag == SHARED_MEM){
        shmdt(mb->storage.shm_addr);
    }
}

/* Only IPC section is timed */
void send(message_t message, mailbox_t* mailbox_ptr){
    struct timespec t0, t1;

    if (mailbox_ptr->flag == MSG_PASSING){
        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.msgText), 0) == -1){
            perror("msgsnd"); exit(1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        g_send_total += delta_sec(t1, t0);

    } else { // SHARED_MEM
        sem_t *sem_empty = sem_open(SEM_EMPTY, O_CREAT, 0666, 1);
        sem_t *sem_full  = sem_open(SEM_FULL,  O_CREAT, 0666, 0);
        if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED){ perror("sem_open"); exit(1); }

        clock_gettime(CLOCK_MONOTONIC, &t0);
        sem_wait(sem_empty);
        memset(mailbox_ptr->storage.shm_addr, 0, MAX_MSG);
        strncpy(mailbox_ptr->storage.shm_addr, message.msgText, MAX_MSG-1);
        sem_post(sem_full);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        g_send_total += delta_sec(t1, t0);

        sem_close(sem_empty);
        sem_close(sem_full);
    }
}

int main(int argc, char **argv){
    if (argc < 3){
        fprintf(stderr, "usage: %s <1|2> <input.txt>\n", argv[0]);
        return 1;
    }
    int mech = atoi(argv[1]);
    const char *path = argv[2];

    mailbox_t mb = {0};
    init_mailbox_sender(&mb, mech);

    FILE *fp = fopen(path, "r");
    if (!fp){ perror("open input"); return 2; }

    char line[MAX_MSG];
    message_t msg = {.mType = 1};

    while (fgets(line, sizeof line, fp)){
        line[strcspn(line, "\r\n")] = 0;
        strncpy(msg.msgText, line, sizeof(msg.msgText)-1);
        send(msg, &mb);
        // 可依作業需求在此列印每筆傳送資訊（不要把 printf 的時間算進 g_send_total）
        // printf("[sender] %s\n", msg.msgText);
    }
    fclose(fp);

    strncpy(msg.msgText, EXIT_TAG, sizeof(msg.msgText)-1);
    send(msg, &mb);

    printf("Total sending time (mechanism=%d): %.6f sec\n", mech, g_send_total);
    close_mailbox_sender(&mb);
    return 0;
}
