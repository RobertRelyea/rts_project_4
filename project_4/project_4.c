#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"

pthread_mutex_t queue_locker;

double time_elapsed(struct timespec *start, struct timespec *current)
{
	double time_s  = current->tv_sec - start->tv_sec;
	double time_ns = current->tv_nsec - start->tv_nsec;
	return time_s + time_ns*1e-9;
}

queue *queue_ptr;

// Enqueueing thread
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


int main(int argc, char *argv[])
{
	pthread_mutex_init(&queue_locker,NULL);
	pthread_mutex_lock(&queue_locker);
	queue_ptr = initialize_queue();
	pthread_mutex_unlock(&queue_locker);


	/*pthread_t enq_thread;
	pthread_t deq_thread;
	pthread_create(&enq_thread, NULL, enqueueThread, NULL);
	pthread_create(&deq_thread, NULL, dequeueThread, NULL);
	// Do some stuff
	pthread_join(enq_thread, NULL);
	pthread_join(deq_thread, NULL)

	pthread_mutex_lock(&queue_locker);
	free_queue(queue_ptr);
	pthread_mutex_unlock(&queue_locker);*/


	struct timespec start;
	struct timespec current;

	clock_gettime(CLOCK_REALTIME, &start);
	clock_gettime(CLOCK_REALTIME, &current);

	printf("start\r\n");

	srand(100);

	while(time_elapsed(&start, &current) < 5.0)//(current_time-start_time < 1.00))
	{
		useconds_t interval = (100 + (rand() % 300))*1000;
		useconds_t cust_wait = (50 + (rand() % 750))*1000;
		usleep(interval);
		pthread_mutex_lock(&queue_locker);
		enqueue(queue_ptr,1,cust_wait);
		pthread_mutex_unlock(&queue_locker);


		clock_gettime(CLOCK_REALTIME, &current);
	}
	print_queue(queue_ptr);
	free_queue(queue_ptr);

}


