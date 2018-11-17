/*
 * queue.h
 *
 *  Created on: Nov 12, 2018
 *      Author: sxs9479
 */

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef QUEUE_H_
#define QUEUE_H_

// Element inside queue
// cust_id:          Per-customer identifier
// teller_id:        Teller that serviced this customer
// service_time:     Time required by teller to service
// birth_time:       System time when this customer entered the queue
// queue_time:       Time spent in the queue by the customer
// teller_wait_time: Teller idle time before this customer came along
// next:             Pointer to the next node in the queue
typedef struct node_t
{
	int cust_id;
	int teller_id;
	useconds_t service_time;
	struct timespec birth_time;
	double queue_time;
	double teller_wait_time;
	struct node_t* next;
}node;


// Queue structure, maintains queue size and head/tail node pointers
typedef struct queue_t
{
	int size;
	node* head;
	node* tail;
}queue;


// Node creation and deletion functions
node* make_node(int cust_id, useconds_t time_interval);
void free_node(node* node_ptr);

// Queue management functions
queue* initialize_queue(void);
void enqueue(queue* queue_ptr, node* node_ptr);
node* dequeue(queue *queue_ptr);
void free_queue (queue* queue_ptr);
void print_queue (queue *queue_ptr);

// Queue statistics functions
int teller_served(queue* queue_ptr, int teller_id);
double average_queue_time(queue* queue_ptr);
double average_service_time(queue* queue_ptr);
double max_queue_time(queue* queue_ptr);
double average_teller_wait_time(queue* queue_ptr);
double max_teller_wait_time(queue* queue_ptr);
double max_service_time(queue* queue_ptr);

#endif /* QUEUE_H_ */
