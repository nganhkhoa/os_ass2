#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */	
	q->size++;
	int insert_place = q->size - 1;
	q->proc[insert_place] = proc;

	while (insert_place != 0) {
		uint32_t parent_place = (insert_place - 1) / 2;

		uint32_t insert_prio = q->proc[insert_place]->priority;
		uint32_t parent_prio = q->proc[parent_place]->priority;
		if (insert_prio >= parent_prio)
			break;

		struct pcb_t * temp = q->proc[insert_place];
		q->proc[insert_place] = q->proc[parent_place];
		q->proc[parent_place] = temp;
		insert_place = parent_place;
	}
}


void heapify(struct queue_t * q, int idx) {
	int left_idx = idx * 2 + 1;
	int right_idx = idx * 2 + 2;
	int min_idx = idx;

	int left_prio;
	int right_prio;
	int min_prio = q->proc[idx]->priority;

	if (left_idx < q->size) {
		left_prio = q->proc[left_idx]->priority;
		if (left_prio < min_prio) {
			min_idx = left_idx;
			min_prio = q->proc[left_idx]->priority;
		}
	}

	if (right_idx < q->size) {
		right_prio = q->proc[right_idx]->priority;
		if (right_prio < min_prio) {
			min_idx = right_idx;
			// min_prio = q->proc[right_idx]->priority;
		}
	}

	if (min_idx == idx)
		return;

	struct pcb_t * temp = q->proc[idx];
	q->proc[idx] = q->proc[min_idx];
	q->proc[min_idx] = temp;

	heapify(q, min_idx);
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */


	if (empty(q))
		return NULL;

	struct pcb_t * ret = q->proc[0];
	// minheapify
	q->proc[0] = q->proc[q->size - 1];
	q->size--;

	heapify(q, 0);

	return ret;
	
}

