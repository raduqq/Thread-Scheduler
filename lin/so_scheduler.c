// Copyright Radu-Stefan Minea 334CA [2022]

#include <stdbool.h>
#include <semaphore.h>

#include "so_scheduler.h"
#include "utils.h"

#define MAX_NUM_THREADS 500

// TODO add descr
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
typedef struct
{
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

// TODO add descr
typedef struct
{
    unsigned int size;
    so_thread *tasks[MAX_NUM_THREADS];
} task_queue;

// TODO add descr
// Scheduler struct
typedef struct
{
    bool init;
    unsigned int events;
    unsigned int quantum;
    so_thread *running_th;

    unsigned int num_threads;
    so_thread *threads[MAX_NUM_THREADS];

    task_queue tq;

    // Semaphore for ending the scheduler thread-safely
    sem_t end_sem;
} so_scheduler;


// TODO add descr
so_thread *peek();
// TODO add descr
void pop();
// TODO add descr
void shift_queue_right(unsigned int pivot);
// TODO add descr
unsigned int get_insert_pos(unsigned int trg_priority);
// TODO add descr
void enqueue(so_thread *t);
/**
 * @brief Scheduler update routine. Schedules next thread or sets the current running
 *
 * @param s scheduler to update
 */
void update_scheduler();
// TODO add descr
void init_thread(so_thread *t, so_handler *func, unsigned int priority);
// TODO add descr
void destroy_thread(so_thread *t);
// TODO add descr
void start_thread(so_thread *t);
/**
 * @brief Routine to execute by every thread
 *
 * @param args pointer to a thread struct
 * @return void* NULL system pthread_exit call
 */
void *thread_routine(void *args);

so_scheduler s;

so_thread *peek()
{
    task_queue *tq;
    unsigned int top_elem_ind;

    // Get task queue
    tq = &(s.tq);

    // Get top elem ind
    top_elem_ind = tq->size - 1;

    return tq->tasks[top_elem_ind];
}

void pop()
{
    task_queue *tq;
    unsigned int top_elem_ind;

    // Get task queue
    tq = &(s.tq);
    // Get top elem ind
    top_elem_ind = tq->size - 1;

    // Pop top elem (mark as NULL)
    tq->tasks[top_elem_ind] = NULL;

    // Update queue size
    tq->size--;
}

void shift_queue_right(unsigned int pivot)
{
    task_queue *tq;
    unsigned int i;

    // Get task queue
    tq = &(s.tq);

    // Shift elements one by one
    for (i = tq->size; i > pivot; i--)
    {
        tq->tasks[i] = tq->tasks[i - 1];
    }
}

unsigned int get_insert_pos(unsigned int trg_priority)
{
    task_queue *tq;

    unsigned int left;
    unsigned int mid;
    unsigned int right;
    unsigned int res;

    so_thread *curr_th;
    unsigned int curr_priorty;

    // Get task queue
    tq = &(s.tq);

    // Init boundaries
    left = 0;
    right = tq->size;

    while (left <= right)
    {
        // Get middle ind
        mid = (left + right) / 2;

        // Get middle elem
        curr_th = tq->tasks[mid];
        curr_priorty = curr_th->priority;

        // Compare elem with target & update boundaries
        if (curr_priorty < trg_priority)
        {
            // Intermediary target found
            res = mid;
            left = mid + 1;
        } else
        {
            right = mid - 1;
        }
    }

    return res;
}

void enqueue(so_thread *t)
{
    task_queue *tq;
    unsigned int insert_pos;

    // Get task queue
    tq = &(s.tq);

    // Get pos to insert new task
    insert_pos = get_insert_pos(t->priority);
    // Make space for the new task
    shift_queue_right(insert_pos);

    // Insert new task
    tq->tasks[insert_pos] = t;
    // Update queue size
    tq->size++;
}

int so_init(unsigned int time_quantum, unsigned int io)
{
    int ret;

    // Invalid call check
    if (s.init || time_quantum == 0 || io > SO_MAX_NUM_EVENTS)
    {
        return -1;
    }

    // Init fields
    s.init = true;
    s.events = io;
    s.quantum = time_quantum;
    s.num_threads = 0;
    s.running_th = NULL;

    // Init task queue
    s.tq.size = 0;

    // Init end semaphore as unlocked
    ret = sem_init(&s.end_sem, 0, 1);
    DIE(ret < 0, "end sem init");

    return 0;
}

void so_end(void)
{
    int ret;
    unsigned int i;

    // Invalid call check
    if (!s.init)
    {
        return;
    }

    // Lock the semaphore for thread-safe ending
    ret = sem_wait(&s.end_sem);
    DIE(ret < 0, "end sem wait");

    // Wait for threads to finish
    for (i = 0; i < s.num_threads; ++i)
    {
        ret = pthread_join(s.threads[i]->tid, NULL);
        DIE(ret < 0, "thread join");
    }

    // Destroy threads struct
    for (i = 0; i < s.num_threads; ++i)
    {
        destroy_thread(s.threads[i]);
    }

    // TODO can be modularized
    // Destroy scheduler
    s.init = false;
    ret = sem_destroy(&s.end_sem);
    DIE(ret < 0, "end sem destroy");
}

tid_t so_fork(so_handler *func, unsigned int priority)
{
    task_queue *tq;
    so_thread *t;

    tq = &(s.tq);

    // Invalid call check
    if (priority > SO_MAX_PRIO || !func)
    {
        return INVALID_TID;
    }

    // Alloc thread
    t = malloc(sizeof(so_thread));
    DIE(t == NULL, "t malloc");

    // Init new thread
    init_thread(t, func, priority);

    // Update scheduler thread list
    s.threads[s.num_threads] = t;
    // Update scheduler thread count
    s.num_threads++;
    
    // Update thread status to ready
    t->status = READY;
    // Schedule new thread
    enqueue(t);

    // If no thread is running
    if (!s.running_th)
    {
        // Update scheduler (look for new threads)
        update_scheduler();
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
    t = s.running_th;
    // Update time left
    new_time_left = t->time_left - 1;
    t->time_left = new_time_left;

    // Update scheduler with new time left
    update_scheduler();

    // Wait for green light from scheduler
    ret = sem_wait(&t->run_sem);
    DIE(ret < 0, "sem_wait");
}

int so_wait(unsigned int io)
{
    so_thread *t;

    // Invalid call check
    if (io >= s.events)
    {
        return -1;
    }

    // Get running thread
    t = s.running_th;
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
    if (io >= s.events)
    {
        return -1;
    }

    tq = &(s.tq);
    resolved_th_cnt = 0;

    for (i = 0; i < s.num_threads; ++i)
    {
        curr_th = s.threads[i];

        // If threads waits for given event and its status is blocked
        if (curr_th->device == io && curr_th->status == BLOCKED)
        {
            // Reset waiting event to default
            curr_th->device = SO_MAX_NUM_EVENTS;

            // Set thread status to ready
            curr_th->status = READY;
            // Schedule thread
            enqueue(curr_th);

            // Update resolved threads count
            resolved_th_cnt++;
        }
    }

    // Resume course of threads
    so_exec();

    // Return number of awakened threads
    return resolved_th_cnt;
}

void update_scheduler()
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

    tq = &(s.tq);
    curr_th = s.running_th;

    // No more threads in queue
    if (tq->size == 0)
    {
        // If current thread finished
        if (curr_th->status == TERMINATED)
        {
            // Trigger scheduler stop
            ret = sem_post(&s.end_sem);
            DIE(ret < 0, "end sem post");
        }

        // Set current thread to run (last thread to be ran)
        sem_post(&curr_th->run_sem);

        // Update done
        return;
    }

    // There are still threads in queue
    next_th = peek();

    // Set scheduler state variables
    is_no_th_running = (s.running_th == NULL);
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
            enqueue(curr_th);
        }

        // Set next thread running
        s.running_th = next_th;
        start_thread(next_th);

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
            enqueue(curr_th);

            // Set next thread running
            s.running_th = next_th;
            start_thread(next_th);

            // Update done
            return;
        }
        // No one better suited to run next than curr_th -> reset curr_th time
        else
        {
            //! replaceble with s->running_th->time_left = s->quantum
            curr_th->time_left = s.quantum;
        }
    }

    // Current thread has no need to be preempted -> set it running
    sem_post(&curr_th->run_sem);
}

void init_thread(so_thread *t, so_handler *func, unsigned int priority)
{
    int ret;

    // Init fields
    t->tid = INVALID_TID;
    t->status = NEW;
    t->handler = func;
    t->time_left = s.quantum;
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

void start_thread(so_thread *t)
{
    int ret;

    // Pop task queue
    pop();

    // Set thread to running
    t->status = RUNNING;
    t->time_left = s.quantum;

    // Unlock running semaphore
    ret = sem_post(&t->run_sem);
    DIE(ret < 0, "run sem post");
}

void *thread_routine(void *args)
{
    int ret;
    so_thread *t;

    t = (so_thread *)args;

    // Wait for green light from scheduler
    ret = sem_wait(&t->run_sem);
    DIE(ret < 0, "run sem wait");

    // Run handler
    t->handler(t->priority);

    // Running done -> update status & scheduler
    t->status = TERMINATED;
    update_scheduler(&s);

    pthread_exit(NULL);
}