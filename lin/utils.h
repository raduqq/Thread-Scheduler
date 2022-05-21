#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <semaphore.h>

#include "so_scheduler.h"
#include "so_thread.h"
#include "task_queue.h"

// TODO add descr
// Scheduler struct
typedef struct
{
    unsigned int events;
    unsigned int quantum;
    so_thread *running_th;

    unsigned int num_threads;
    so_thread *threads[MAX_NUM_THREADS];

    task_queue *tq;

    // Semaphore for ending the scheduler thread-safely
    sem_t end_sem;
} so_scheduler;

#define DIE(assertion, call_description)               \
    do                                                 \
    {                                                  \
        if (assertion)                                 \
        {                                              \
            fprintf(stderr, "(%s, %s, %d): ",          \
                    __FILE__, __FUNCTION__, __LINE__); \
            perror(call_description);                  \
            exit(EXIT_FAILURE);                        \
        }                                              \
    } while (0)

#endif
