#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL) return;
        if (q->size >= MAX_QUEUE_SIZE) {
                return;
        }

        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (q == NULL || q->size == 0)
        return NULL;

        int best = 0;

        for (int i = 1; i < q->size; i++) {
                if (q->proc[i]->priority < q->proc[best]->priority) {
                best = i;
                }
        }

        struct pcb_t *chosen = q->proc[best];

        for (int i = best; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }

        q->size--;

        return chosen;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        if (q == NULL || q->size == 0 || proc == NULL)
        return NULL;

        int idx = -1;
        for (int i = 0; i < q->size; i++) {
                if (q->proc[i] == proc) {
                idx = i;
                break;
                }
        }

        if (idx == -1)
                return NULL;   // không tìm thấy

        /* Dồn các phần tử phía sau lên để lấp chỗ trống */
        for (int i = idx; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }

        q->size--;

        return proc;
}