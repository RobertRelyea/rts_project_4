#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"

pthread_mutex_t queue_locker;
pthread_cond_t bank_open;
pthread_cont_t customer_ready;

pthread_mutex_t num_free_tellers_locker;
int num_free_tellers = 0;

double time_elapsed(struct timespec *start, struct timespec *current)
{
	double time_s  = current->tv_sec - start->tv_sec;
	double time_ns = current->tv_nsec - start->tv_nsec;
	return time_s + time_ns*1e-9;
}

queue *queue_ptr;

// Enqueuing thread
void *enqueueThread(void *arg)
{
	int i;
	for(i=0;i<10;i++)
	{
		pthread_mutex_lock(&queue_locker);
		enqueue(queue_ptr, i, 0);
		pthread_mutex_unlock(&queue_locker);
		printf("%d\r\n",i);
	}
	printf("\n Size of queue : %d\r\n",queue_ptr->size);
}

//Dequeue Thread
void *dequeueThread(void *arg)
{
	int deq_val;
	while(queue_ptr->size)
		{
			printf("\n Size of queue : %d\r\n",queue_ptr->size);
			pthread_mutex_lock(&queue_locker);
			deq_val=dequeue(queue_ptr);
			pthread_mutex_unlock(&queue_locker);
			printf ("\n Dequeue value is: %d\r \n", deq_val);
		}
	printf("\n Size of queue : %d\r\n",queue_ptr->size);
}

//Teller thread

void *Teller (void *arg)
{

	pthread_cond_wait(bank_open);

	pthread_mutex_lock( &queue_update );
	while( num_free_tellers == 0 )
	{
		pthread_cond_wait( &customer_ready, &queue_update);
		//operations
		pthread_cond_signal( &customer_ready );
		pthread_mutex_unlock( &queue_update );
	}
}


int main(int argc, char *argv[])
{

	pthread_mutex_init(&queue_locker,NULL);
	pthread_mutex_lock(&queue_locker);
	queue_ptr = initialize_queue();
	pthread_mutex_unlock(&queue_locker);

	pthread_t teller1_thread;
	pthread_create(&Teller, NULL, teller1_thread, NULL);


	struct timespec start;
	struct timespec current;

	clock_gettime(CLOCK_REALTIME, &start);
	clock_gettime(CLOCK_REALTIME, &current);

	printf("start\r\n");

	srand(100);

	// Open Bank

	while(time_elapsed(&start, &current) < 5.0)//(current_time-start_time < 1.00))
	{
		// Generate time interval between customer arrivals
		useconds_t interval = (100 + (rand() % 300))*1000;
		// Generate customer processing time
		useconds_t cust_wait = (50 + (rand() % 750))*1000;
		usleep(interval);
		// Lock queue and enqueue new customer
		pthread_mutex_lock(&queue_locker);
		enqueue(queue_ptr,1,cust_wait);
		pthread_mutex_unlock(&queue_locker);

		// Check if there are any tellers available for customers
		pthread_mutex_lock(num_free_tellers_locker);
		if(num_free_tellers > 0)
		{
			// There is a teller free, signal to grab customer
			pthread_cond_signal(&customer_ready);
		}
		pthread_mutex_unlock(num_free_tellers_locker);

		clock_gettime(CLOCK_REALTIME, &current);
	}

	// Close bank

	pthread_join(teller1_thread, NULL);

	print_queue(queue_ptr);
	free_queue(queue_ptr);

}


