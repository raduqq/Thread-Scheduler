#include "so_thread.h"

void init_thread(so_scheduler *s, so_thread *t, void *thread_routine, so_handler *func, unsigned int priority)
{
    int ret;

    // Init fields
    t->tid = INVALID_TID;
    t->status = NEW;
    t->handler = func;
    t->time_left = s->quantum;
    t->priority = priority;
    t->device = SO_MAX_NUM_EVENTS;

    // Init running semaphore as already locked
    ret = sem_init(&t->run_sem, 0, 0);
    DIE(ret < 0, "run sem init");

    // Create & start thread
    ret = pthread_create(&t->tid, NULL, &thread_routine, t);
    DIE(ret < 0, "thread create");
}

void destroy_thread(so_thread *t)
{
    int ret;

    ret = sem_destroy(&t->run_sem);
    DIE(ret < 0, "run sem destroy");

    free(t);
}

void start_thread(so_thread *t, so_scheduler *s, task_queue *tq)
{
    int ret;

    // Pop task queue
    pop(tq);

    // Set thread to running
    t->status = RUNNING;
    t->time_left = s->quantum;

    // Unlock running semaphore
    ret = sem_post(&t->run_sem);
    DIE(ret < 0, "run sem post");
}