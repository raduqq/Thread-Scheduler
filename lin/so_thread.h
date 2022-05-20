#ifndef SO_THREAD_H
#define SO_THREAD_H

#include "utils.h"

// TODO modify
#define MAX_NUM_THREADS 1000

// Thread status structure
typedef enum
{
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} so_thread_status;

// Thread structure
typedef struct {
    // ID
    tid_t tid;
    // Running status
    so_thread_status status;

    // Semaphore to signal when to run and when to sleep
    sem_t run_sem;
    // Handler to execute
    so_handler *handler;
    // Running time left
    unsigned int time_left;

    // Priority for scheduling
    unsigned int priority;

    // Event
    unsigned int device;
} so_thread;

// TODO: add descr
void init_thread(so_scheduler *s, so_thread *t, so_handler *f, unsigned int priority);
// TODO: add descr
void destroy_thread(so_thread *t);
// TODO: add descr
void start_thread(so_thread *t, so_scheduler *s, task_queue *tq);
// TODO: add descr
void *thread_routine(void *args);

#endif