#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>

/* ---- Shared constants for both mechanisms ---- */
#define MSG_PASSING 1
#define SHARED_MEM  2
#define MAX_MSG     1024
#define EXIT_TAG    "__EXIT__"

/* ---- TA minimal mailbox: flag + storage ---- */
typedef struct {
    int flag;  // 1 for message passing, 2 for shared memory
    union {
        int  msqid;     // System V message queue id
        char *shm_addr; // Shared memory address
    } storage;
} mailbox_t;

/* ---- Message wrapper (System V requires long first) ---- */
typedef struct {
    long mType;
    char msgText[MAX_MSG];
} message_t;

void send(message_t message, mailbox_t* mailbox_ptr);
