/*
 * queue.c
 *
 *  Created on: Nov 12, 2018
 *      Author: sxs9479
 */

#include "queue.h"

// Initialize queue structure
// Allocate memory for queue structure
// Initialize size to zero, set head and tail pointers to zero
queue* initialize_queue(void)
{
	queue *queue_ptr;
	queue_ptr = malloc(sizeof(queue));
	queue_ptr->size = 0;
	queue_ptr->head = 0;
	queue_ptr->tail = 0;
	return queue_ptr;
}

void enqueue(queue* queue_ptr, int cust_id, int time_interval)
{
	node* node_ptr = malloc(sizeof(node));
	node_ptr->cust_id = cust_id;
	node_ptr->time_interval = time_interval;
	// Last element in queue
	node_ptr->next = 0;

	// Check if the queue is empty
	if(queue_ptr->size == 0)
	{
		// Node becomes the head of the queue
		queue_ptr->head = node_ptr;

	}
	else // Queue not empty, set node as next for old tail
	{
		queue_ptr->tail->next = node_ptr;
	}
	queue_ptr->tail= node_ptr;
	queue_ptr->size++;
}

int dequeue(queue *queue_ptr)
{
	int ret_val;
	node *node_ptr;
	if(queue_ptr->size!=0)
	{
		ret_val = queue_ptr->head->cust_id;
		node_ptr = queue_ptr->head;
		queue_ptr->head = node_ptr->next;
		free(node_ptr);
		queue_ptr->size--;
	}
	return ret_val;
}

void print_queue (queue *queue_ptr)
{
	node *node_ptr;
	for (node_ptr= queue_ptr->head ; node_ptr!=0 ; node_ptr=node_ptr->next)
	{
		printf ("\nQueue value: %d\r\n \n Interval: %d\r" , node_ptr->cust_id,node_ptr->time_interval);
	}
}

void free_queue (queue* queue_ptr)
{
	while (queue_ptr->size)
	{
		dequeue(queue_ptr);
	}
	free(queue_ptr);
}
