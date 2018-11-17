#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "queue.h"

#define NUM_TELLERS (2)
#define SIM_TIME (5) // 7 hr * 60 mins/hr * 0.1 seconds/min = 42

// Shared variable mutexes
pthread_mutex_t cust_locker;
pthread_mutex_t old_cust_locker;

// Conditionals
pthread_cond_t bank_open;

// Semaphores
sem_t cust_count_sem;

// Customer Queues
queue *cust_queue;
queue *old_cust_queue;

// Teller threads
pthread_t teller_threads[NUM_TELLERS];

// Teller arguments
struct teller_args{
	int teller_id;
};

struct teller_args teller_arg_array[NUM_TELLERS];

double time_elapsed(struct timespec *start, struct timespec *current)
{
	double time_s  = current->tv_sec - start->tv_sec;
	double time_ns = current->tv_nsec - start->tv_nsec;
	return time_s + time_ns*1e-9;
}


void print_stats()
{
	int teller = 1;
	printf("Total customers served: %d\n", old_cust_queue->size);

	for(;teller <= NUM_TELLERS; ++teller)
	{
		printf("\tCustomers served by Teller %d: %d\n",
				teller, teller_served(old_cust_queue, teller));
	}
}


//Teller thread
void *Teller (void *arg)
{
	struct timespec current;
	struct teller_args *my_args;
	my_args = (struct teller_args *) arg;

	// Run teller thread
	while(1)
	{
		sem_wait(&cust_count_sem);
		pthread_mutex_lock(&cust_locker);
		node* customer = dequeue(cust_queue);
		pthread_mutex_unlock(&cust_locker);

		// If there are no customers in the queue, exit
		if(customer == 0)
		{
			// Post to cust_count semaphore to pass the message to the other threads
			sem_post(&cust_count_sem);
			// Exit thread
			return;
		}

		clock_gettime(CLOCK_REALTIME, &current);

		// Update customer queue wait time
		customer->queue_time = time_elapsed(&(customer->birth_time), &current);

		// Process customer
		usleep(customer->service_time);

		// Update customer stats
		customer->teller_id = my_args->teller_id;

		// Place customer into old_cust_queue
		pthread_mutex_lock(&old_cust_locker);
		enqueue(old_cust_queue, customer);
		pthread_mutex_unlock(&old_cust_locker);
	}
}


int main(int argc, char *argv[])
{
    // Initialize Mutexes
	pthread_mutex_init(&cust_locker,NULL);
	pthread_mutex_init(&old_cust_locker,NULL);

	// Initialize Semaphores
	sem_init(&cust_count_sem, 0, 0);

	// Initialize queues
	pthread_mutex_lock(&cust_locker);
	cust_queue = initialize_queue();
	pthread_mutex_unlock(&cust_locker);

	pthread_mutex_lock(&old_cust_locker);
	old_cust_queue = initialize_queue();
	pthread_mutex_unlock(&old_cust_locker);

//	// Start threads
	int teller = 0;
	for(;teller < NUM_TELLERS; ++teller)
	{
		teller_arg_array[teller].teller_id = teller + 1;

		pthread_create(&teller_threads[teller], NULL, Teller,
				       (void *) &teller_arg_array[teller]);
	}

	// Timekeeping
	struct timespec start;
	struct timespec current;

	clock_gettime(CLOCK_REALTIME, &start);
	clock_gettime(CLOCK_REALTIME, &current);

	printf("Bank open!\n");

	srand(100);

	// Open Bank
	int cust_id = 0;
	while(time_elapsed(&start, &current) < SIM_TIME)//(current_time-start_time < 1.00))
	{
		// Generate time interval between customer arrivals
		useconds_t interval = (100 + (rand() % 300))*1000;
		// Generate customer processing time
		useconds_t cust_wait = (50 + (rand() % 750))*1000;
		usleep(interval);

		// Create new customer node
		node* node_ptr = make_node(cust_id, cust_wait);
		cust_id++;

		// Lock queue and enqueue new customer
		pthread_mutex_lock(&cust_locker);
		enqueue(cust_queue, node_ptr);
		pthread_mutex_unlock(&cust_locker);

		// Post to cust_count semaphore
		sem_post(&cust_count_sem);

		// Update current time
		clock_gettime(CLOCK_REALTIME, &current);
	}

	// Close bank
	sem_post(&cust_count_sem);

//	pthread_mutex_lock(&cust_locker);
//	pthread_mutex_lock(&old_cust_locker);

	teller = 0;
	for(;teller < NUM_TELLERS; ++teller)
	{
		pthread_join(teller_threads[teller], NULL);
		printf("Teller %d joined\n", teller + 1);
	}

	printf("\nCustomer Queue:\n");
	print_queue(cust_queue);

	printf("\nServed Customer Queue:\n");
	print_queue(old_cust_queue);

	print_stats();

	free_queue(old_cust_queue);
	free_queue(cust_queue);

//	pthread_mutex_unlock(&cust_locker);
//	pthread_mutex_unlock(&old_cust_locker);

	return 0;

}


