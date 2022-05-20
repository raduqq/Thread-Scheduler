#include "task_queue.h"

task_queue *create_task_queue()
{
    task_queue *tq;

    // Alloc memory
    tq = calloc(1, sizeof(task_queue));
    DIE(tq == NULL, "calloc create tq");

    // Init fields
    tq->size = 0;

    return tq;
}

void pop(task_queue *tq)
{
    int top_elem_ind;

    // Get top elem ind
    top_elem_ind = tq->size - 1;
    // Pop top elem (mark as NULL)
    tq->tasks[top_elem_ind] = NULL;

    // Update queue size
    tq->size--;
}

void shift_queue_right(task_queue *tq, unsigned int pivot)
{
    int i;

    // Shift elements one by one
    for (i = tq->size; i > pivot; i--)
    {
        tq->tasks[i] = tq->tasks[i - 1];
    }
}

unsigned int get_insert_pos(task_queue *tq, unsigned int trg_priority)
{
    unsigned int left;
    unsigned int mid;
    unsigned int right;
    unsigned int res;

    so_thread *curr_th;
    unsigned int curr_priorty;

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
        }
        else
        {
            right = mid - 1;
        }
    }

    return res;
}

// TODO: nu uita sa dai s.queue[i]->status = READY inainte de enqueue
void enqueue(task_queue *tq, so_thread *t)
{
    unsigned int insert_pos;

    // Get pos to insert new task
    insert_pos = get_insert_pos(tq, t->priority);
    // Make space for the new task
    shift_queue_right(tq, insert_pos);

    // Insert new task
    tq->tasks[insert_pos] = t;
    // Update queue size
    tq->size++;
}