#include "safequeue.h"
#include <errno.h>

// create new priority queue
PriorityQueue* create_queue(int maxSize) {
    PriorityQueue *queue = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    if(queue == NULL) {
        printf("error while mallocing queue\n");
        free(queue);
        exit(EXIT_FAILURE);
    }

    queue->maxSize = maxSize;
    queue->currSize = 0;

    queue->head = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->notEmpty, NULL);

    return queue;
}

// add job to the priority queue
Job *add_work(PriorityQueue* queue, Job *job) { 
    printf("about to acquire lock\n");
    pthread_mutex_lock(&queue->lock);
    printf("acquired lock\n");
    
    
    if (queue->currSize >= queue->maxSize) {
        pthread_mutex_unlock(&queue->lock);
        printf("max size\n");
        return NULL;
    }

    // add job to queue -- made head or loop through based off priority
    Job *curr = queue->head;
    if(curr == NULL || job->priority > curr->priority) {
        job->next = curr;
        queue->head = job;
    } else { // loop through to place job based off priority

        while(curr->next != NULL && job->priority < curr->next->priority) {
            if(job->priority < curr->priority) { // higher priority means closer to front !
                curr = curr->next;
            }

            printf("inside loop\n");
        }
        job->next = curr->next;
        curr->next = job;
        
    }
    queue->currSize++;

    // signal queue not empty !!
    printf("about to signal\n"); 
    pthread_cond_signal(&queue->notEmpty);
    pthread_mutex_unlock(&queue->lock);
    
    printf("about to return from add_job\n");
    return job;
}

// get highest priority job from job queue
Job get_work(PriorityQueue* queue) {
    pthread_mutex_lock(&queue->lock);

    // wait for queue to have job(s) if empty
    while(queue->currSize == 0) {
        pthread_cond_wait(&queue->notEmpty, &queue->lock);
    }

    // remove job
    Job *head = queue->head;
    queue->head = head->next;

    queue->currSize--;
    pthread_mutex_unlock(&queue->lock);

    return *head;
}

Job *get_work_nonblocking(PriorityQueue* queue) {
    pthread_mutex_lock(&queue->lock);

    // wait for queue to have job(s) if empty
    if(queue->currSize == 0) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }
    
    // remove job
    Job *head = queue->head;
    queue->head = head->next;

    queue->currSize--;
    pthread_mutex_unlock(&queue->lock);

    return head;
}