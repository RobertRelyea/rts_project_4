/*
 * queue.c
 *
 *  Created on: Nov 12, 2018
 *      Author: sxs9479
 */

#include "queue.h"

// Create a new node with given field values.
// Returns a pointer to the newly allocated node.
node* make_node(int cust_id, useconds_t service_time)
{
	node* node_ptr = malloc(sizeof(node));
	node_ptr->cust_id = cust_id;
	node_ptr->teller_id = 0;
	node_ptr->service_time = service_time;
	clock_gettime(CLOCK_REALTIME, &(node_ptr->birth_time));
	return node_ptr;
}


// Free allocated memory for a given node.
void free_node(node* node_ptr)
{
	free(node_ptr);
}


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


// Enqueue a given node into the given queue
void enqueue(queue* queue_ptr, node* node_ptr)
{
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


node* dequeue(queue *queue_ptr)
{
	node *node_ptr = 0;
	if(queue_ptr->size!=0)
	{
		node_ptr = queue_ptr->head;
		queue_ptr->head = node_ptr->next;
		queue_ptr->size--;
	}
	return node_ptr;
}


void print_queue (queue *queue_ptr)
{
	node *node_ptr;
	for (node_ptr= queue_ptr->head ; node_ptr!=0 ; node_ptr=node_ptr->next)
	{
		printf("Queue value: %d", node_ptr->cust_id);
		printf("\tTeller: %d",node_ptr->teller_id);
		printf("\tInterval: %d",node_ptr->service_time);
		printf("\tQueue Time: %f\n",node_ptr->queue_time);
	}
}


void free_queue (queue* queue_ptr)
{
	while (queue_ptr->size)
	{
		node* node_ptr = dequeue(queue_ptr);
		free_node(node_ptr);
	}
	free(queue_ptr);
}

//         QUEUE STATS

int teller_served(queue* queue_ptr, int teller_id)
{
	node *node_ptr;
	int teller_count = 0;
	for (node_ptr=queue_ptr->head ; node_ptr!=0 ; node_ptr=node_ptr->next)
	{
		if (node_ptr->teller_id == teller_id)
			teller_count++;
	}

	return teller_count;
}
