#include <stdlib.h>

#include "so_scheduler.h"
#include "utils.h"

static so_scheduler *s;

void destroy_scheduler(so_scheduler *s)
{
    int ret;

    ret = sem_destroy(&s->end_sem);
    DIE(ret < 0, "end sem destroy");

    free(s->tq);
    free(s);
}

int so_init(unsigned int time_quantum, unsigned int io)
{
    int ret;

    // Invalid call check
    if (s || time_quantum == 0 || io > SO_MAX_NUM_EVENTS)
    {
        return -1;
    }

    // Alloc scheduler structure
    s = calloc(1, sizeof(so_scheduler));
    DIE(s == NULL, "scheduler calloc");

    // Init fields
    s->events = io;
    s->quantum = time_quantum;
    s->num_threads = 0;
    s->running_th = NULL;

    // Init task queue
    s->tq = create_task_queue();

    // Init end semaphore as unlocked
    ret = sem_init(&s->end_sem, 0, 1);
    DIE(ret < 0, "end sem init");

    return 0;
}

void so_end(void)
{
    int ret;
    int i;

    // Invalid call check
    if (!s)
    {
        return;
    }

    // Lock the semaphore for thread-safe ending
    ret = sem_wait(&s->end_sem);
    DIE(ret < 0, "end sem wait");

    // Wait for threads to finish
    for (i = 0; i < s->num_threads; ++i)
    {
        ret = pthread_join(s->threads[i]->tid, NULL);
        DIE(ret < 0, "thread join");
    }

    // Destroy threads struct
    for (i = 0; i < s->num_threads; ++i)
    {
        destroy_thread(s->threads[i]);
    }

    // Destroy scheduler
    destroy_scheduler(s);
}

/**
 * TODO
 * @brief Scheduler update routine. Schedules next thread or sets it running
 *
 * @param s scheduler to update
 */
void update_scheduler(so_scheduler *s)
{
    int ret;

    task_queue *tq;
    so_thread *curr_th;
    so_thread *next_th;

    tq = s->tq;
    curr_th = s->running_th;

    // No more threads in queue
    //! TODO LEFTOVER HERE. gandeste ce a facut baiatul si reformuleaza
    if (tq->size == 0)
    {
    }
}