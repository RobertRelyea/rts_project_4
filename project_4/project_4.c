#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "queue.h"

#define NUM_TELLERS (3)
#define SIM_TIME (42) // 7 hr * 60 mins/hr * 0.1 seconds/min = 42

// Shared variable mutexes
pthread_mutex_t cust_locker;
pthread_mutex_t old_cust_locker;

// Semaphores
sem_t cust_count_sem;
sem_t bank_closed;

// Customer Queues
queue *cust_queue;
queue *old_cust_queue;

// Teller threads
pthread_t teller_threads[NUM_TELLERS];

// Teller status
struct teller_status
{
	struct timespec next_break;
	useconds_t break_length;
	int is_free;
	int on_break;
	int breaks;
	sem_t wake;
	pthread_mutex_t thread_status_locker;
};
struct teller_status teller_status_array[NUM_TELLERS];

// Teller arguments
struct teller_args
{
	int teller_id;
};
struct teller_args teller_arg_array[NUM_TELLERS];

// Bank statistics
int max_queue_depth = 0;


// Returns the difference between two timespecs in seconds
double time_elapsed(struct timespec *start, struct timespec *current)
{
	double time_s  = (double)(current->tv_sec) - (double)(start->tv_sec);
	double time_ns = (double)(current->tv_nsec) - (double)(start->tv_nsec);
	return time_s + time_ns*1e-9;
}


void print_stats()
{
	int teller = 1;
	// 1
	printf("Total customers served: %d\n", old_cust_queue->size);
	// 2
	for(;teller <= NUM_TELLERS; ++teller)
	{
		printf("\tCustomers served by Teller %d: %d\n",
				teller, teller_served(old_cust_queue, teller));
	}
	// 3
	printf("Average customer queue time: %f seconds\n",
			average_queue_time(old_cust_queue));
	// 4
	printf("Average teller wait time: %f seconds\n",
			average_teller_wait_time(old_cust_queue));
	// 5
	printf("Average customer service time: %f seconds\n",
			average_service_time(old_cust_queue));
	// 6
	printf("Maximum customer queue time: %f seconds\n",
			max_queue_time(old_cust_queue));
	// 7
	printf("Maximum teller wait time: %f seconds\n",
			max_teller_wait_time(old_cust_queue));
	// 8
	printf("Maximum customer service time: %f seconds\n",
			max_teller_wait_time(old_cust_queue));
	// 9
	printf("Maximum customer queue depth: %d customers\n",
			max_queue_depth);
	// 10
	int teller_idx = 0;
	for(;teller_idx < NUM_TELLERS ;teller_idx++)
	{
		int breaks = teller_status_array[teller_idx].breaks;
		printf("Teller %d break number: %d\n", teller_idx + 1, breaks);

	}
}


void gen_customer_time(struct timespec* ts_ptr)
{
	// Generate time interval between customer arrivals
	int interval = (100 + (rand() % 300)) * 1000000;
	clock_gettime(CLOCK_REALTIME, ts_ptr);
	// Check if generated time rolls over into seconds
	if(ts_ptr->tv_nsec + interval > 1e9)
	{
		ts_ptr->tv_nsec += interval - 1e9;
		ts_ptr->tv_sec++;
	}
	else
	{
		ts_ptr->tv_nsec += interval;
	}
}

void gen_break_time(struct timespec* ts_ptr)
{
	time_t interval = 3 + (rand() % 3); // Between 30 and 60 minutes
	clock_gettime(CLOCK_REALTIME, ts_ptr);
	ts_ptr->tv_sec += interval;
	return ts_ptr;
}


useconds_t gen_break_length()
{
	// Generate break length for teller
	useconds_t interval = (100 + (rand() % 300))*1000;
	return interval;
}

// Initialize all status members for each teller.
//
void initializeTellerStatuses()
{
	int teller_idx = 0;
	for(;teller_idx < NUM_TELLERS ;teller_idx++)
	{
		// Generate a new break time for this teller
		gen_break_time(&teller_status_array[teller_idx].next_break);
		// Generate random break length
		teller_status_array[teller_idx].break_length = gen_break_length();
		// Busy and break flags
		teller_status_array[teller_idx].is_free = 1;
		teller_status_array[teller_idx].on_break = 0;
		teller_status_array[teller_idx].breaks = 0;
		// Struct access mutex
		pthread_mutex_init(&teller_status_array[teller_idx].thread_status_locker,NULL);
		// Teller thread wake semaphore
		sem_init(&teller_status_array[teller_idx].wake, 0, 0);
	}
}

//Teller thread
void *teller (void *arg)
{
	struct timespec current, start;
	struct teller_args *my_args;
	my_args = (struct teller_args *) arg;


	// Run teller thread
	while(1)
	{
		// Wait until we get a wake signal
//		printf("Waiting for wake on teller %d\n", my_args->teller_id);
		sem_wait(&teller_status_array[my_args->teller_id - 1].wake);
//		printf("Wake received on teller %d\n", my_args->teller_id);

		// Retrieve current time
		clock_gettime(CLOCK_REALTIME, &current);

		int bank_closed_val = 0;
		int cust_count_val = 0;

		sem_getvalue(&bank_closed, &bank_closed_val);
		sem_getvalue(&cust_count_sem, &cust_count_val);

		if(bank_closed_val && cust_count_val == 0)
		{
			// Exit thread
			printf("Teller %d exiting\n", my_args->teller_id);
			return;
		}

		// Check if we are on break
		if ((time_elapsed(&teller_status_array[my_args->teller_id - 1].next_break,&current) >= 0))
		{
			printf("Teller %d going on break\n", my_args->teller_id );
			// Load break time
			pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
			useconds_t break_length = teller_status_array[my_args->teller_id - 1].break_length;
			pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);

			// Sleep for break
			usleep(break_length);

			// Update on_break status
			pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
			teller_status_array[my_args->teller_id - 1].on_break = 0;
			teller_status_array[my_args->teller_id - 1].breaks++;
			gen_break_time(&teller_status_array[my_args->teller_id - 1].next_break);
			teller_status_array[my_args->teller_id - 1].break_length = gen_break_length();

			pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);

			printf("Teller %d off break\n", my_args->teller_id );

			// Go back to waiting for wake signal
			continue;
		}

		// Not on break, check if customer is waiting

		// Track teller waiting time
		clock_gettime(CLOCK_REALTIME, &start); // ChANGE ME


		// Wait until a customer is available
//		printf("Waiting for customer on teller %d\n", my_args->teller_id);
		sem_wait(&cust_count_sem);

		// Set is_free status to 0
		pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
		teller_status_array[my_args->teller_id - 1].is_free = 0;
		pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);

		pthread_mutex_lock(&cust_locker);
		node* customer = dequeue(cust_queue);
		pthread_mutex_unlock(&cust_locker);

		// Retrieve current time
		clock_gettime(CLOCK_REALTIME, &current);

		// Update customer time statistics
		customer->queue_time = time_elapsed(&(customer->birth_time),
											&current);
		customer->teller_wait_time = time_elapsed(&start, &current);

		// Process customer
		usleep(customer->service_time);

		// Update customer statistics
		customer->teller_id = my_args->teller_id;

		// Place customer into old_cust_queue
		pthread_mutex_lock(&old_cust_locker);
		enqueue(old_cust_queue, customer);
		pthread_mutex_unlock(&old_cust_locker);

		// Set is_free status to 0
		pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
		teller_status_array[my_args->teller_id - 1].is_free = 1;
		pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);

	}
}

int main(int argc, char *argv[])
{
    // Initialize Mutexes
	pthread_mutex_init(&cust_locker,NULL);
	pthread_mutex_init(&old_cust_locker,NULL);

	// Initialize Semaphores
	sem_init(&cust_count_sem, 0, 0);
	sem_init(&bank_closed, 0, 0);

	// Initialize queues
	pthread_mutex_lock(&cust_locker);
	cust_queue = initialize_queue();
	pthread_mutex_unlock(&cust_locker);

	pthread_mutex_lock(&old_cust_locker);
	old_cust_queue = initialize_queue();
	pthread_mutex_unlock(&old_cust_locker);

	// Initialize all teller status flags
	initializeTellerStatuses();

	// Start threads
	int teller_idx = 0;
	for(;teller_idx < NUM_TELLERS; ++teller_idx)
	{
		teller_arg_array[teller_idx].teller_id = teller_idx + 1;

		pthread_create(&teller_threads[teller_idx], NULL, teller,
				       (void *) &teller_arg_array[teller_idx]);
	}

	// Time keeping for bank open interval
	struct timespec start;
	struct timespec current;

	// Customer generation time
	struct timespec next_customer_ts;
	gen_customer_time(&next_customer_ts);


	clock_gettime(CLOCK_REALTIME, &start);
	clock_gettime(CLOCK_REALTIME, &current);

	printf("Bank open!\n");

	srand(100);

	// Open Bank
	int cust_id = 0;
	while(time_elapsed(&start, &current) < SIM_TIME)
	{

		// Check all teller break times
		teller_idx = 0;
		for(;teller_idx < NUM_TELLERS ;teller_idx++)
		{
			pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);

			// Check if the break time for this teller has elapsed and if
			// a break wake signal has not already been sent to this teller
			if(time_elapsed(&teller_status_array[teller_idx].next_break,&current) >= 0 &&
						    teller_status_array[teller_idx].on_break <= 0)
			{
				teller_status_array[teller_idx].on_break = 1;
				sem_post(&teller_status_array[teller_idx].wake);
			}

			pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
		}

		// Check if it is time for a new customer to enter the queue
		if(time_elapsed(&next_customer_ts, &current) >= 0)
		{

			// Generate customer processing time
			useconds_t cust_wait = (50 + (rand() % 750))*1000;

			// Create new customer node
			node* node_ptr = make_node(cust_id, cust_wait);
			cust_id++;

			// Lock queue and enqueue new customer
			pthread_mutex_lock(&cust_locker);
			enqueue(cust_queue, node_ptr);

			if(cust_queue->size > max_queue_depth)
				max_queue_depth = cust_queue->size;

			pthread_mutex_unlock(&cust_locker);

			// Post to cust_count semaphore
			sem_post(&cust_count_sem);
//			int sem_value = 0;
//			sem_getvalue(&cust_count_sem, &sem_value);
//			printf("Cust count: %d\n", sem_value);
			int wake_sent =0;

			for(teller_idx = 0;teller_idx < NUM_TELLERS ;teller_idx++)
			{
				pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);
				// If the teller flag is set, wake the teller
				if (teller_status_array[teller_idx].is_free &&
					teller_status_array[teller_idx].on_break <= 0 && wake_sent != 1)
				{
					sem_post(&teller_status_array[teller_idx].wake);
					teller_status_array[teller_idx].is_free = 0;
					wake_sent =1;
				}

				pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
			}
			// if none of the tellers are free, assign a random teller to the next customer
			if (wake_sent==0)
			{
				pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);
				// Generate a teller index between 1 and NUM_TELLERS
				int teller_num = (rand()%(NUM_TELLERS));
				sem_post(&teller_status_array[teller_num].wake);
				wake_sent=1;
				pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
			}

			// Schedule next customer
			gen_customer_time(&next_customer_ts);
		}
		// Update current time
		clock_gettime(CLOCK_REALTIME, &current);
	}

	printf("Bank Closed!\n");
	sem_post(&bank_closed);


	// Close bank
	for(teller_idx = 0;teller_idx < NUM_TELLERS ;teller_idx++)
	{
		pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);
		// If the teller flag is set, wake the teller
		sem_post(&teller_status_array[teller_idx].wake);
		pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
	}

//
//	pthread_mutex_lock(&teller_status_array[0].thread_status_locker);
//	// If the teller flag is set, wake the teller
//	sem_post(&teller_status_array[0].wake);
//	pthread_mutex_unlock(&teller_status_array[0].thread_status_locker);



	for(teller_idx = 0;teller_idx < NUM_TELLERS; ++teller_idx)
	{
		pthread_join(teller_threads[teller_idx], NULL);
		printf("Teller %d joined\n", teller_idx + 1);
	}

	pthread_mutex_lock(&cust_locker);
	pthread_mutex_lock(&old_cust_locker);

	printf("\nCustomer Queue:\n");
	print_queue(cust_queue);

	printf("\nServed Customer Queue:\n");
	print_queue(old_cust_queue);

	print_stats();

	//	usleep(1e6);

	int sem_value = 0;
	sem_getvalue(&cust_count_sem, &sem_value);
	printf("Cust count: %d\n", sem_value);
	sem_getvalue(&teller_status_array[0].wake, &sem_value);
	printf("Teller 1 wake: %d\n", sem_value);
	sem_getvalue(&teller_status_array[1].wake, &sem_value);
	printf("Teller 2 wake: %d\n", sem_value);
	sem_getvalue(&teller_status_array[2].wake, &sem_value);
	printf("Teller 3 wake: %d\n", sem_value);
	usleep(1e5);
	usleep(1e5);
	usleep(1e5);

	free_queue(old_cust_queue);
	free_queue(cust_queue);

	pthread_mutex_unlock(&cust_locker);
	pthread_mutex_unlock(&old_cust_locker);

	return 0;

}


