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
	int longest_break;
	int shortest_break;
	int total_break;
	sem_t wake;
	sem_t assigned;
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
			average_queue_time(old_cust_queue) * 600);
	// 4
	printf("Average customer service time: %f seconds\n",
			average_service_time(old_cust_queue) * 600);
	// 5
	printf("Average teller wait time: %f seconds\n",
			average_teller_wait_time(old_cust_queue) * 600);
	// 6
	printf("Maximum customer queue time: %f seconds\n",
			max_queue_time(old_cust_queue) * 600);
	// 7
	printf("Maximum teller wait time: %f seconds\n",
			max_teller_wait_time(old_cust_queue) * 600);
	// 8
	printf("Maximum customer service time: %f seconds\n",
			max_service_time(old_cust_queue) * 600);
	// 9
	printf("Maximum customer queue depth: %d customers\n",
			max_queue_depth);
	// 10
	int teller_idx = 0;
	for(;teller_idx < NUM_TELLERS ;teller_idx++)
	{
		int breaks = teller_status_array[teller_idx].breaks;
		printf("Teller %d break number: %d\n", teller_idx + 1, breaks);
		if (breaks != 0)
		{
			double shortest_break = (double)(teller_status_array[teller_idx].shortest_break)/1000000;
			printf("\tTeller %d's shortest break: %f seconds\n", teller_idx + 1, shortest_break * 600);

			double longest_break =(double)( teller_status_array[teller_idx].longest_break)/1000000;
			printf("\tTeller %d's longest break: %f seconds\n", teller_idx + 1, longest_break * 600);

			double avg_break = ((teller_status_array[teller_idx].total_break)/(double)(teller_status_array[teller_idx].breaks))/1000000;
			printf("\tTeller %d's average break : %f seconds\n" , teller_idx + 1, avg_break * 600);
		}

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
}


useconds_t gen_break_length()
{
	// Generate break length for teller
	useconds_t interval = (100 + (rand() % 300))*1000;
	return interval;
}

// Initialize all status members for each teller.
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
		teller_status_array[teller_idx].longest_break = 0;
		teller_status_array[teller_idx].shortest_break = 600000;
		teller_status_array[teller_idx].total_break = 0;
		// Struct access mutex
		pthread_mutex_init(&teller_status_array[teller_idx].thread_status_locker,NULL);
		// Teller thread wake semaphore
		sem_init(&teller_status_array[teller_idx].wake, 0, 0);
		sem_init(&teller_status_array[teller_idx].assigned, 0, 0);
	}
}

//Teller thread
void *teller (void *arg)
{
	struct timespec current, start;
	struct teller_args *my_args;
	my_args = (struct teller_args *) arg;

	double wait_time = 0;

	// Track teller waiting time
	clock_gettime(CLOCK_REALTIME, &start);

	// Run teller thread
	while(1)
	{
		// Wait until we get a wake signal
		sem_wait(&teller_status_array[my_args->teller_id - 1].wake);

		// Not on break, check if customer is waiting
		int bank_closed_val = 0;
		int assigned_val = 0;

		sem_getvalue(&bank_closed, &bank_closed_val);
		sem_getvalue(&teller_status_array[my_args->teller_id - 1].assigned, &assigned_val);

		// Check if bank closed and we have no assigned customers
		if(bank_closed_val && assigned_val == 0)
		{
			// Exit thread
			return;
		}

		// Check if we are on break
		if (teller_status_array[my_args->teller_id - 1].on_break)
		{
			// Load break time
			pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
			useconds_t break_length = teller_status_array[my_args->teller_id - 1].break_length;
			pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);

			// Sleep for break
			usleep(break_length);
			wait_time -= (double)(break_length) / 1e6;

			// Update on_break status
			pthread_mutex_lock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
			teller_status_array[my_args->teller_id - 1].on_break = 0;
			teller_status_array[my_args->teller_id - 1].breaks++;
			teller_status_array[my_args->teller_id - 1].total_break += break_length;

			if(teller_status_array[my_args->teller_id - 1].longest_break < break_length)
				teller_status_array[my_args->teller_id - 1].longest_break = break_length;

			if(teller_status_array[my_args->teller_id - 1].shortest_break > break_length)
				teller_status_array[my_args->teller_id - 1].shortest_break = break_length;

			gen_break_time(&teller_status_array[my_args->teller_id - 1].next_break);
			teller_status_array[my_args->teller_id - 1].break_length = gen_break_length();

			pthread_mutex_unlock(&teller_status_array[my_args->teller_id - 1].thread_status_locker);
			// Go back to waiting for wake signal
			continue;
		}


		// Wait until a customer is available
		sem_wait(&teller_status_array[my_args->teller_id - 1].assigned);
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
		customer->teller_wait_time = time_elapsed(&start, &current) + wait_time;
		wait_time = 0;

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

		// Track teller waiting time
		clock_gettime(CLOCK_REALTIME, &start);
	}
}

int main(int argc, char *argv[])
{
    // Initialize Mutexes
	pthread_mutex_init(&cust_locker,NULL);
	pthread_mutex_init(&old_cust_locker,NULL);

	// Initialize Semaphores
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


			int wake_sent =0;


			// See who is free
			int num_free_tellers = 0;
			int free_tellers[NUM_TELLERS];
			for(teller_idx = 0;teller_idx < NUM_TELLERS ;teller_idx++)
			{
				pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);
				// If the teller flag is set, wake the teller
				if (teller_status_array[teller_idx].is_free &&
					teller_status_array[teller_idx].on_break <= 0 && wake_sent != 1)
				{
					free_tellers[num_free_tellers] = teller_idx;
					num_free_tellers++;
//					sem_post (&teller_status_array[teller_idx].assigned);
//					sem_post(&teller_status_array[teller_idx].wake);
//					teller_status_array[teller_idx].is_free = 0;
//					wake_sent =1;
				}
				pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
			}


			if (num_free_tellers != 0)
			{
				int teller_to_assign = rand() % num_free_tellers;
				pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);

				sem_post (&teller_status_array[free_tellers[teller_to_assign]].assigned);
				sem_post(&teller_status_array[free_tellers[teller_to_assign]].wake);
				teller_status_array[free_tellers[teller_to_assign]].is_free = 0;

				pthread_mutex_unlock(&teller_status_array[teller_idx].thread_status_locker);
			}

			// if none of the tellers are free, assign a random teller to the next customer
			else
			{
				pthread_mutex_lock(&teller_status_array[teller_idx].thread_status_locker);
				// Generate a teller index between 1 and NUM_TELLERS
				int teller_num = (rand()%(NUM_TELLERS));
				sem_post(&teller_status_array[teller_num].assigned);
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
		// If the teller flag is set, wake the teller
		sem_post(&teller_status_array[teller_idx].wake);
	}

	// Join all teller threads
	for(teller_idx = 0;teller_idx < NUM_TELLERS; ++teller_idx)
	{
		pthread_join(teller_threads[teller_idx], NULL);
	}

	pthread_mutex_lock(&cust_locker);
	pthread_mutex_lock(&old_cust_locker);

	print_stats();

	free_queue(old_cust_queue);
	free_queue(cust_queue);

	pthread_mutex_unlock(&cust_locker);
	pthread_mutex_unlock(&old_cust_locker);

	return 0;

}


