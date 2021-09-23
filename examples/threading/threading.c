/** 
Author : Mehul
completed - modified given assignment code 
*/ 
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
//Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

pthread_t thread_id;   
void* threadfunc(void* thread_param)
{

	// TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
	// hint: use a cast like the one below to obtain thread arguments from your parameter
	
	//initializing thread function with passed parameters
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;
	DEBUG_LOG("initializing thread function with passed parameters\n");
	DEBUG_LOG("wait to obtain mutex : %d ms\n",thread_func_args->wait_to_obtain_ms);
	DEBUG_LOG("wait to release mutex :%d ms\n ",thread_func_args->wait_to_release_ms);
	
	//check if time value is valid - non negative 
	if(thread_func_args->wait_to_obtain_ms < 0)
		usleep(0);

	//wait pre mutex lock
	//ussleep argument usec => n *1000
	usleep((thread_func_args->wait_to_obtain_ms)*1000);

	//obtain mutex lock 
	int ret = pthread_mutex_lock(thread_func_args->mutex);
	perror("pthread_mutex_lock");
	if(ret!= 0)
	{
		ERROR_LOG("pthread_mutex_lock failed");
		return thread_param;
	}

	//wait for release mutex lock 
	if(thread_func_args->wait_to_release_ms < 0)
		usleep(0);

	//wait for release mutex lock
	//ussleep argument usec => n *1000
	usleep((thread_func_args->wait_to_release_ms)*1000);

	//Unlock - release the mutex lock 
	ret = pthread_mutex_unlock(thread_func_args->mutex);
	perror("pthread_mutex_lock");
	if(ret!= 0)
	{
		ERROR_LOG("pthread_mutex_unlock failed");
		return thread_param;
	}

	//Return thread_complete_success = true 
	//both if mutex lock and release successful
	thread_func_args->thread_complete_success = true;

	DEBUG_LOG("Thread completed");

	//exit thread with thread parameters
	return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	/**
	 * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
	 * using threadfunc() as entry point.
	 *
	 * return true if successful.
	 * 
	 * See implementation details in threading.h file comment block
	 */
	struct thread_data *thread_data = malloc(sizeof(struct thread_data));

	//initializing thread data structure with input arguments
	thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
	thread_data->wait_to_release_ms = wait_to_release_ms;
	thread_data->mutex = mutex; 
	thread_data->thread_complete_success = false;

	DEBUG_LOG("initialized thread data structure with input arguments");

	//creating thread with above thread parameter and thread start fn
	int ret = pthread_create(&thread_id, NULL, &threadfunc, thread_data);    
	perror("pthread_create");
	if(ret != 0)
	{	
		//pthread create Error occurred 
		return false;
	}

	//return thread Id to caller thread 
	*thread = thread_id;
	return true;
}


