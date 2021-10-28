/*
Author : Mehul Patel 
creating Socket server to handle multiple conneciton using multithreading
refernces : 
https://beej.us/guide/bgnet/html/
https://www.geeksforgeeks.org/socket-programming-cc/
*/
//valgrind passing successfully w/o leak 

//TODO :  rework on sending at BUFFER_SIZE length  
// logic for sending at broken packet required for future asmnt		


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
#include "queue.h"
#include <pthread.h>

#define PORT_NUMBER "9000"
#define BACKLOG 6
#define BUFFER_SIZE 100  //max buffer size for the packet transfer
#define TIMER_BUFFER_SIZE 100
#define FILE_PATH "/var/tmp/aesdsocketdata"



int socket_fd, client_fd;
int outputfile_fd;
int terminate_on_error = 0; 
int terminate_on_signal= 0; 

//thread parameter structure 
typedef struct thread_data{
	pthread_t thread_index;
	int client_fd;
	char *rxbuf;
	char *txbuf;
	char client_ip[20];
	sigset_t signal_mask;
	pthread_mutex_t *mutex;

	bool thread_complete_success;
	/*Set to true  if the thread completed with success,
	  false if an error occurred. */
}thread_data_t;
pthread_mutex_t file_mutex_lock;
//timer_t timer_id;

/*-----------------------------------------------------------------*/
typedef struct slist_data_s slist_data_t;
struct slist_data_s 
{
	thread_data_t thread_param;
	SLIST_ENTRY(slist_data_s) entries;
};
slist_data_t *node=NULL;
SLIST_HEAD(slisthead, slist_data_s) head;
/*-----------------------------------------------------------------*/
// adding support for IPv6  -  get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* signal handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_DEBUG, "Caught Signal, exiting");
		if(shutdown(socket_fd, SHUT_RDWR))
		{
			perror("shutdown");
			syslog(LOG_ERR,"Failed on shutdown()");
		}

		/*
		   if(timer_delete(timer_id)==-1)
		   {
		   perror("timer_delete");
		   syslog(LOG_ERR,"failed timer delete");
		   }
		   */
		terminate_on_signal = 1;		 
		return;		 
	}
}

//not used - cleaned up in main 
void delete_linkedlist()
{
	//free linked list
	while(!SLIST_EMPTY(&head))
	{
		node = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head,entries);
		free(node);
	}		
}
//thread hadler function for handling connection with client -  rx and send data 
void* thread_func(void* thread_param)
{
	thread_data_t *tparam = (thread_data_t*)thread_param;
	int received_bytes, write_bytes, read_bytes, sent_bytes;
	int rx_loc = 0;
	int count = 1;
	char ch;
	int index = 0;
	int newline_index =0;
	int curr_sendbytes =0;
	int total_txbuffsize = BUFFER_SIZE;

	// allocate memory for buffers
	tparam->rxbuf = (char *)malloc(BUFFER_SIZE*sizeof(char));
	if(tparam->rxbuf == NULL)
	{
		syslog(LOG_ERR,"Error while doing malloc\n");		 
		terminate_on_error = 1; 
		return NULL;
	}

	tparam->txbuf = (char *)malloc(BUFFER_SIZE*sizeof(char));
	if(tparam->txbuf == NULL)
	{
		syslog(LOG_ERR,"Error while doing malloc\n");
		terminate_on_error =1;
		return NULL;
	}

	/*----------------------------------------------------------*/
	//receive data 
	do
	{
		received_bytes = recv(tparam->client_fd, tparam->rxbuf + rx_loc, BUFFER_SIZE, 0); //sizeof(buffer)
		if(received_bytes <= 0)
		{
			syslog(LOG_ERR,"Error while receiving\n");
		}

		rx_loc += received_bytes;

		// increase rxbuf size dynamically if required
		count += 1;
		tparam->rxbuf = (char *)realloc(tparam->rxbuf, count*BUFFER_SIZE*(sizeof(char)));
		if(tparam->rxbuf == NULL)
		{
			close(tparam->client_fd);
			syslog(LOG_ERR,"Error while doing realloc\n");
			terminate_on_error =1;
			return NULL;			 
		}
	}while(strchr(tparam->rxbuf, '\n') == NULL);
	received_bytes = rx_loc;

	/*----------------------------------------------------------*/

	int rc;
	rc = pthread_mutex_lock(tparam->mutex); //lock mutex
	if(rc != 0)
	{
		perror("Mutex lock failed\n");
		terminate_on_error =1;
		return NULL;

	} 	

	// block the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_BLOCK, &tparam->signal_mask, NULL);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while blocking the signals SIGINT, SIGTERM");
		terminate_on_error =1;
		return NULL;						
	}			

	// write to the file
	write_bytes = write(outputfile_fd, tparam->rxbuf, received_bytes);
	if(write_bytes < 0)
	{
		syslog(LOG_ERR,"Error in writing bytes\n");
		close(tparam->client_fd);
		terminate_on_error =1;
		return NULL;

	}

	rc = pthread_mutex_unlock(tparam->mutex); // unlock mutex
	if(rc != 0)
	{
		perror("Mutex unlock failed\n");
		terminate_on_error =1;
		return NULL;

	}

	// unblock the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_UNBLOCK, &tparam->signal_mask, NULL);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while unblocking the signals SIGINT, SIGTERM");
		terminate_on_error =1;
		return NULL;

	}
	/*----------------------------------------------------------------*/
	int size  = lseek(outputfile_fd,0,SEEK_END);
	tparam->txbuf = (char *)realloc(tparam->txbuf, size*sizeof(char));
	if(tparam->txbuf == NULL)
	{
		close(tparam->client_fd);
		syslog(LOG_ERR,"Error while doing realloc\n");
		terminate_on_error =1;
		return NULL;

	}

	rc = pthread_mutex_lock(tparam->mutex); //lock mutex
	if(rc != 0)
	{
		syslog(LOG_ERR,"Mutex lock failed\n");
		terminate_on_error =1;
		return NULL;

	}

	// block the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_BLOCK, &tparam->signal_mask, NULL);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while blocking the signals SIGINT, SIGTERM");
		terminate_on_error =1;
		return NULL;

	}	
	/*---------------------------------------------------------------*/
	//read data from file send packet by packet to client
	lseek(outputfile_fd, 0, SEEK_SET); //set cursor to start
	
	while((read_bytes = read(outputfile_fd,&ch,1)) > 0)
	{
		if(read_bytes < 0)
		{
			close(tparam->client_fd);
			syslog(LOG_ERR,"Error in reading bytes\n");
			terminate_on_error =1;
			return NULL;
		}
		tparam->txbuf[index] = ch;
		
		if(tparam->txbuf[index]=='\n')
		{
			curr_sendbytes = (index + 1) - newline_index;
			sent_bytes = send(tparam->client_fd, tparam->txbuf+newline_index, curr_sendbytes, 0);//send bytes
			if(sent_bytes < 0)
			{
				close(tparam->client_fd);
				syslog(LOG_ERR,"Error in sending bytes\n");
				terminate_on_error =1; 
				return NULL;				 
			}
			newline_index = index + 1;
		}
		index++;
		if(index >= total_txbuffsize)
		{
		    total_txbuffsize += BUFFER_SIZE;
		    tparam->txbuf=realloc(tparam->txbuf,sizeof(char)*total_txbuffsize);
		}
	}
	/*-----------------------------------------------------------------*/ 
	rc = pthread_mutex_unlock(tparam->mutex); // unlock mutex
	if(rc != 0)
	{
		syslog(LOG_ERR,"Mutex unlock failed\n");
		terminate_on_error =1;
		tparam->thread_complete_success = false;
		return NULL;

	}				

	// unblock the signals SIGINT and SIGTERM
	rc = sigprocmask(SIG_UNBLOCK, &tparam->signal_mask, NULL);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while unblocking the signals SIGINT, SIGTERM");
		terminate_on_error =1;
		return NULL;

	}

	// close connection
	close(tparam->client_fd);	 
	syslog(LOG_DEBUG, "Closed connection from %s\n", tparam->client_ip);
	//printf("Closed connection from %s\n", tparam->client_ip);
	free(tparam->rxbuf);
	free(tparam->txbuf);
	tparam->thread_complete_success = true;

	return NULL;	
}

//add timespecs and store in result
static void timespec_add(const struct timespec *ts_1, const struct timespec *ts_2, struct timespec *result)
{
	result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
	result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
	if( result->tv_nsec > 1000000000L )
	{
		result->tv_nsec -= 1000000000L;
		result->tv_sec++;
	}
}


//thread to write time stamp at every 10 seconds 
static void timer_threadfn(union sigval sigval)
{
	char time_buffer[TIMER_BUFFER_SIZE];
	time_t current_time;
	struct tm *timer_info;
	time(&current_time);
	timer_info = localtime(&current_time);

	/* Get timestamp in formatted string as per requirement */
	int timer_buffer_size = strftime(time_buffer,TIMER_BUFFER_SIZE,"timestamp:%a, %d %b %Y %T %z\n",timer_info);

	if(pthread_mutex_lock(&file_mutex_lock)!=0)
	{
		perror("pthread_mutex_lock");
		syslog(LOG_ERR, "Mutex lock failed\n");
	}

	// write to file
	int timer_writebytes = write(outputfile_fd,time_buffer,timer_buffer_size);
	if(timer_writebytes == -1)
	{
		syslog(LOG_ERR,"Error in writing time to file\n");
		terminate_on_error =1;
		return;
	}
	if(pthread_mutex_unlock(&file_mutex_lock)!=0)
	{
		perror("pthread_mutex_unlock");
		syslog(LOG_ERR, "Mutex lock failed\n");
		terminate_on_error =1;
		return;
	}
}

int main(int argc, char*argv[])
{
	int daemon_mode = 0;
	int rc;
	struct addrinfo hints, *sockaddrinfo; 
	openlog(NULL, 0, LOG_USER);



	/*-------------------------------------------------------------------*/	
	//check if daemon mode specified at cmd line  
	if(argc == 2)
	{
		if(strcmp("-d", argv[1]) == 0)
		{
			daemon_mode = 1;
		}	
	}
	/*-------------------------------------------------------------------*/	
	/* Setting up signal handlers SIGINT and SIGTERM */
	if (signal (SIGINT, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR,"Could not setup single handler for SIGINT\n");
		closelog();
		return -1;
	}

	// Register signal_handler as our signal handler for SIGTERM
	if (signal (SIGTERM, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR,"Could not setup single handler for SIGTERM\n");
		closelog();
		return -1;
	}

	/*-------------------------------------------------------------------*/		
	/* Creating signal set ,and add signal SIGINT SIGTERM into created empty set*/ 
	sigset_t signal_mask; 
	rc = sigemptyset(&signal_mask);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error sigemptyset()");
		closelog();
		return -1;
	}

	rc = sigaddset(&signal_mask, SIGINT);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while adding SIGINT to sigaddset()");
		closelog();
		return -1;
	}

	rc = sigaddset(&signal_mask, SIGTERM);
	if(rc == -1)
	{
		syslog(LOG_ERR,"Error while adding SIGTERM to sigaddset()");
		closelog();
		return -1;
	}
	/*-------------------------------------------------------------------*/	
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0)
	{
		syslog(LOG_ERR, "Error Creating socket\n");
		terminate_on_error =1;
		goto clean_and_exit;
	}

	/*-------------------------------------------------------------------*/	
	int reuse_addr = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1)
	{
		syslog(LOG_ERR, "setsockopt");
		terminate_on_error =1;
		goto clean_and_exit;
	}

	/*-----------------------------------------------------------------------*/	
	/* call getaddrinfo to get socket addresses info linkelist to use for bind()
	   it converts hostnames/IP addresses into a dynamically allocated linked list of struct addrinfo structures 
	   arguments - hostname- domain name ,service,hints, structure to store result into 
	   returns result into - addrinfo structures, each of which contains an Internet address linkelist that can be specified
	   */
	memset(&hints, 0 , sizeof (hints));

	hints.ai_family = AF_INET;   
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; 

	if(getaddrinfo(NULL, PORT_NUMBER, &hints, &sockaddrinfo) != 0)
	{
		syslog(LOG_ERR, "Error getting address info\n");
		terminate_on_error =1;
		goto clean_and_exit;		 
	}
	/*------bind-------------------------------------------------------------*/	

	rc = bind(socket_fd, sockaddrinfo->ai_addr, sockaddrinfo->ai_addrlen);
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in binding\n");
		terminate_on_error =1;
		goto clean_and_exit;
	}

	freeaddrinfo(sockaddrinfo); // free sockaddrinfo linked list

	/*-------------------------------------------------------------------*/	
	//initialize mutex
	rc = pthread_mutex_init(&file_mutex_lock,NULL);
	if(rc != 0)
	{
		syslog(LOG_ERR,"error in initializing the mutex\n");
		terminate_on_error =1;
		goto clean_and_exit;
		exit(EXIT_FAILURE);
	}

	/*-------------------------------------------------------------------*/	
	pid_t pid;
	//if daemon mode provided at the cmd-line fork daemon child 
	// and exectue remaining program as daemon 
	if(daemon_mode)
	{
		pid = fork();

		if(pid < 0 )
		{
			syslog(LOG_ERR, "Forking error\n");
		}
		if(pid > 0)
		{
			syslog(LOG_DEBUG, "Daemon created\n");
			exit(EXIT_SUCCESS);
		}
		if(pid == 0)
		{
			/* setting up daemon */

			//set session id 
			pid_t sid = setsid();
			if(sid < 0)
			{
				syslog(LOG_ERR, "Error in setting sid\n");
				terminate_on_error =1;
				goto clean_and_exit;
			}
			//change current directory to root 
			rc=chdir("/");
			if(rc== -1)				
			{
				syslog(LOG_ERR, "chdir");
				syslog(LOG_ERR, "chdir : could not change directory to root ");
				terminate_on_error = 1;
				goto clean_and_exit; //clean_close_and_exit;
			}

			//redirect stdin, sdtout, stderror to /dev/null 
			open("/dev/null", O_RDWR);
			perror("open");
			dup(0);
			dup(0);						 
		}
	}
	timer_t timer_id;
	/*-------------------------------------------------------------------------------*/	
	//set up timer functionality
	if((daemon_mode == 0) || (pid == 0))
	{
		struct sigevent sev;
		memset(&sev,0,sizeof(struct sigevent));

		// setup timer
		sev.sigev_notify = SIGEV_THREAD;
		sev.sigev_notify_function = timer_threadfn;


		//create a timer
		rc = timer_create(CLOCK_MONOTONIC,&sev,&timer_id);
		if(rc == -1) 
		{
			syslog(LOG_ERR,"Error in timer_create()\n");
			terminate_on_error =1;
			goto clean_and_exit;
		}

		/*-------------------------------------------------------------------------------*/
		//get start time time-stamp in struct 
		struct timespec start_time;
		rc = clock_gettime(CLOCK_MONOTONIC,&start_time);
		if(rc == -1)
		{
			syslog(LOG_ERR,"Error in clock_gettime()\n");
			terminate_on_error =1;
			goto clean_and_exit;
		} 
		/*-------------------------------------------------------------------------------*/

		//timer interval specificaition
		struct itimerspec itimerspec;
		itimerspec.it_interval.tv_sec = 10;
		itimerspec.it_interval.tv_nsec = 0;

		timespec_add(&start_time, &itimerspec.it_interval, &itimerspec.it_value);  //add timer specs 	


		//arm the timer for the absolute time specified in itimerspec 		
		rc = timer_settime(timer_id, TIMER_ABSTIME, &itimerspec, NULL);
		if(rc == -1 )
		{
			syslog(LOG_ERR,"Error in timer_settime()\n");
			terminate_on_error =1; 
			goto clean_and_exit;
		} 
	}
	/*-------------------------------------------------------------------------------*/
	// listen
	rc = listen(socket_fd, BACKLOG);
	if(rc < 0)
	{
		syslog(LOG_ERR,"Error while listening to socket\n");
		terminate_on_error =1; 
		goto clean_and_exit;
	}
	/*-------------------------------------------------------------------------------*/  
	//open in append or  creating file /var/tmp/aesdsocketdata file if it doesn’t exist.
	//outputfile_fd = open(FILE_PATH, O_CREAT | O_RDWR | O_APPEND, 0744);
	outputfile_fd = open(FILE_PATH, O_CREAT | O_RDWR, 0644);
	if(outputfile_fd < 0)
	{
		syslog(LOG_ERR, "Error: file specified as argument couldn't be opened(outputfile_fd < 0)");
		terminate_on_error =1;
		goto clean_and_exit;
	}

	SLIST_INIT(&head); //initialize linked list

	//datastructure for client socket address  
	struct sockaddr_in clientsockaddr;
	socklen_t addr_size;
	addr_size = sizeof(clientsockaddr);

	while(!(terminate_on_error) && !(terminate_on_signal))
	{
		/* accept a connection on a socket, creates a new connected socket, and returns a new file descrtor */
		client_fd = accept(socket_fd, (struct sockaddr *)&clientsockaddr, &addr_size);
		if(client_fd < 0)
		{
			syslog(LOG_ERR,"Error while accpeting incomming connection\n");			 
			goto clean_and_exit;
			terminate_on_error =1;
			break;
		}
		/* print accepted client IP address */
		char *client_ip_address = inet_ntoa(clientsockaddr.sin_addr);	 //	 convert the IP address to a string
		syslog(LOG_DEBUG,"Accepted connection from %s", client_ip_address);

		/*-----------------------------------------------------------------------*/ 
		//create node for each connection for spawning a new thread to handle the connection
		//and create thread with particular parameters	
		node = (slist_data_t*)malloc(sizeof(slist_data_t)); //creating new node handle new connection 		
		if(node == NULL)
		{
			syslog(LOG_ERR,"Error while doing malloc\n");
			terminate_on_error =1;
			goto clean_and_exit;
			break;
		}

		//store thread parameters
		node->thread_param.client_fd = client_fd;
		node->thread_param.thread_complete_success = false;
		node->thread_param.signal_mask = signal_mask;
		node->thread_param.mutex = &file_mutex_lock;
		strcpy(node->thread_param.client_ip, client_ip_address);

		SLIST_INSERT_HEAD(&head, node, entries); // insert created node in linkelist

		//create a thread
		rc = pthread_create(&(node->thread_param.thread_index), NULL, (void *) &thread_func, (void *) &(node->thread_param)); // create pthread
		if(rc != 0)
		{
			syslog(LOG_ERR,"Error while creating pthread\n");
			terminate_on_error =1;
			goto clean_and_exit;			 
		}

		/*-----------------------------------------------------------------------*/ 
		//search through the linked list to check completed threads
		SLIST_FOREACH(node, &head, entries) 
		{
			if(node->thread_param.thread_complete_success == true)
			{
				// join completed thread
				pthread_join(node->thread_param.thread_index, NULL); 	// waits for thread to complete operataion						 
			}
		}
	}


	/*----------------------------------------------------------------------------*/
	//do clean up and closing file descriptors and exit 
clean_and_exit:

	if(timer_delete(timer_id)==-1)
	{
		perror("timer_delete");
		syslog(LOG_ERR,"failed timer delete");
	}
	close(socket_fd);
	close(outputfile_fd);
	closelog();
	remove(FILE_PATH);

	//cancel threads which are not completed and free associated pointers
	SLIST_FOREACH(node,&head,entries)
	{
		if (node->thread_param.thread_complete_success != true)
		{
			pthread_cancel(node->thread_param.thread_index);
			free(node->thread_param.rxbuf);
			free(node->thread_param.txbuf);				 
		}
	}

	//destroy mutex
	int rc2  = pthread_mutex_destroy(&file_mutex_lock);
	if(rc2 != 0)
	{
		syslog(LOG_ERR,"failed in destroying mutex\n");
	}

	//free linked list
	while(!SLIST_EMPTY(&head))
	{
		node = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head,entries);
		free(node);
	}

	if(terminate_on_signal)
		exit (EXIT_SUCCESS);
	if(!terminate_on_error)
		return 0;
	else 
		return -1;
}

