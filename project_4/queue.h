/*
 * queue.h
 *
 *  Created on: Nov 12, 2018
 *      Author: sxs9479
 */

#ifndef QUEUE_H_
#define QUEUE_H_

// Element inside queue
typedef struct node_t
{
	int cust_id;
	int time_interval;
	struct node_t* next;
}node;

// Queue structure, maintains queue size and head/tail node pointers
typedef struct queue_t
{
	int size;
	node* head;
	node* tail;
}queue;

queue* initialize_queue(void);

void enqueue(queue* queue_ptr, int cust_id, int time_interval);

int dequeue(queue *queue_ptr);

void free_queue (queue* queue_ptr);


#endif /* QUEUE_H_ */
