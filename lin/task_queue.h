#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "utils.h"

// TODO: modify
#define Q_CAP 1000

typedef struct
{
    unsigned int size;
    so_thread *tasks[Q_CAP];
} task_queue;

// TODO add descr
task_queue *create_task_queue();
// TODO add descr
void pop(task_queue *tq);
// TODO add descr
void shift_queue_right(task_queue *tq, unsigned int pivot);
// TODO: add descr: get cea mai mare prio mai mica decat trg_prio: binary search
unsigned int get_insert_pos(task_queue *tq, unsigned int trg_priority);
// TODO add descr
void enqueue(task_queue *tq, so_thread *t);

#endif