#include <stdlib.h>

#include "so_scheduler.h"
#include "utils.h"

static so_scheduler *s;

// TODO add descr
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
    unsigned int i;

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

tid_t so_fork(so_handler *func, unsigned int priority)
{
    int ret;
    task_queue *tq;
    so_thread *t;

    tq = s->tq;

    // Invalid call check
    if (priority > SO_MAX_PRIO || !func)
    {
        return INVALID_TID;
    }

    // Alloc thread
    t = calloc(1, sizeof(so_thread));
    DIE(t == NULL, "t calloc");

    // Init new thread
    init_thread(s, t, thread_routine, func, priority);

    // Update scheduler thread list
    s->threads[s->num_threads] = t;
    // Update scheduler thread count
    s->num_threads++;
    // Update thread status to ready
    t->status = READY;
    // Schedule new thread
    enqueue(tq, t);

    // If no thread is running
    if (!s->running_th)
    {
        // Update scheduler (look for new threads)
        update_scheduler(s);
    }
    // There is a thread currently running
    else
    {
        // Run the thread
        so_exec();
    }

    // Return ID of new thread
    return t->tid;
}

void so_exec(void)
{
    int ret;
    so_thread *t;

    unsigned int new_time_left;

    // Get running thread
    t = s->running_th;
    // Update time left
    new_time_left = t->time_left - 1;
    t->time_left = new_time_left;

    // Update scheduler with new time left
    update_scheduler(s);

    // Wait for green light from scheduler
    ret = sem_wait(&t->run_sem);
    DIE(ret < 0, "sem_wait");
}

int so_wait(unsigned int io)
{
    so_thread *t;

    // Invalid call check
    if (io >= s->events || io < 0)
    {
        return -1;
    }

    // Get running thread
    t = s->running_th;
    // Update thread event
    t->device = io;
    // Set thread status to blocked
    t->status = BLOCKED;

    // Resume course of threads
    so_exec();

    return 0;
}

int so_signal(unsigned int io)
{
    unsigned int i;

    task_queue *tq;
    so_thread *curr_th;
    int resolved_th_cnt;

    // Invalid call check
    if (io >= s->events || io < 0)
    {
        return -1;
    }

    tq = s->tq;
    resolved_th_cnt = 0;

    for (i = 0; i < s->num_threads; ++i)
    {
        curr_th = s->threads[i];

        // If threads waits for given event and its status is blocked
        if (curr_th->device == io && curr_th->status == BLOCKED)
        {
            // Reset waiting event to default
            curr_th->device = SO_MAX_NUM_EVENTS;

            // Set thread status to ready
            curr_th->status = READY;
            // Schedule thread
            enqueue(tq, curr_th);

            // Update resolved threads count
            resolved_th_cnt++;
        }
    }

    // Resume course of threads
    so_exec();

    // Return number of awakened threads
    return resolved_th_cnt;
}
/**
 * @brief Scheduler update routine. Schedules next thread or sets the current running
 *
 * @param s scheduler to update
 */
void update_scheduler(so_scheduler *s)
{
    int ret;

    task_queue *tq;
    so_thread *curr_th;
    so_thread *next_th;

    // True if no thread is running; else false
    bool is_no_th_running;
    // True if current thread is blocked or terminated; else false
    bool is_th_blocked_or_done;
    // True if next thread has higher priority than current thread; else false
    bool is_higher_prio_avail;
    // True if current thread has no time left on processor
    bool has_th_time_expired;

    tq = s->tq;
    curr_th = s->running_th;

    // No more threads in queue
    if (tq->size == 0)
    {
        // If current thread finished
        if (curr_th->status == TERMINATED)
        {
            // Trigger scheduler stop
            ret = sem_post(&s->end_sem);
            DIE(ret < 0, "end sem post");
        }

        // Set current thread to run (last thread to be ran)
        sem_post(&curr_th->run_sem);

        // Update done
        return;
    }

    // There are still threads in queue
    next_th = peek(tq);

    // Set scheduler state variables
    is_no_th_running = (s->running_th == NULL);
    is_th_blocked_or_done = curr_th ? (curr_th->status == BLOCKED || curr_th->status == TERMINATED) : false;
    is_higher_prio_avail = curr_th ? (curr_th->priority < next_th->priority) : false;
    has_th_time_expired = curr_th ? (curr_th->time_left <= 0) : false;

    // If we need to set a different thread running than the current one
    if (is_no_th_running || is_th_blocked_or_done || is_higher_prio_avail)
    {
        // If next thread has higher priority
        if (is_higher_prio_avail)
        {
            // Schedule current thread
            curr_th->status = READY;
            enqueue(tq, curr_th);
        }

        // Set next thread running
        s->running_th = next_th;
        start_thread(next_th, s, tq);

        // Update done
        return;
    }

    // Current thread is still running, so we check if we should run it anymore
    if (has_th_time_expired)
    {
        // Thread expired -> set next running through round robin
        if (curr_th->priority == next_th->priority)
        {
            // Schedule current thread
            curr_th->status = READY;
            enqueue(tq, curr_th);

            // Set next thread running
            s->running_th = next_th;
            start_thread(next_th, s, tq);

            // Update done
            return;
        }
        // No one better suited to run next than curr_th -> reset curr_th time
        else
        {
            //! replaceble with s->running_th->time_left = s->quantum
            curr_th->time_left = s->quantum;
        }
    }

    // Current thread has no need to be preempted -> set it running
    sem_post(&curr_th->run_sem);
}

/**
 * @brief Routine to execute by every thread
 *
 * @param args pointer to a thread struct
 * @return void* NULL system pthread_exit call
 */
void *thread_routine(void *args)
{
    int ret;
    so_thread *t;

    t = (so_thread *)t;

    // Wait for green light from scheduler
    ret = sem_wait(&t->run_sem);
    DIE(ret < 0, "run sem wait");

    // Run handler
    t->handler(t->priority);

    // Running done -> update status & scheduler
    t->status = TERMINATED;
    update_scheduler(s);

    pthread_exit(NULL);
}