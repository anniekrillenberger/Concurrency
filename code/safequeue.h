#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H


typedef struct job {
    int priority;
    int client_fd;
    int listener_port;
    int delay;
    char *path;
    int bytes_read;
    char* read_buffer;
    struct job *next;
} Job;

typedef struct {
    int maxSize;
    int currSize;
    Job *head;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
} PriorityQueue;

typedef struct {
    pthread_t listener;
    int port;
} ListenerThread;

// functions for priority queue
PriorityQueue* create_queue(int maxSize);
Job *add_work(PriorityQueue* queue, Job *job);
Job get_work(PriorityQueue* queue);
Job *get_work_nonblocking(PriorityQueue* queue);

#endif