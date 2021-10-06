/*
Author : Mehul Patel 
creating Socket server for assignment -5 
refernces : 
https://beej.us/guide/bgnet/html/
https://www.geeksforgeeks.org/socket-programming-cc/
*/
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

#define PORT_NUMBER "9000"
#define BACKLOG 6
#define BUFFER_SIZE 100 //max buffer size for the packet transfer
#define FILE_PATH "/var/tmp/aesdsocketdata"

int socket_fd, client_fd;
int outputfile_fd;
struct addrinfo hints, *sockaddrinfo;
char *tx_buf;
char *rx_buf;


/* close every all file descriptors - files free buffers, shutdown socket*/
void close_all(void)
{	 
	free(rx_buf);
	free(tx_buf);
	closelog();
	close(outputfile_fd);	
	close(client_fd);
	shutdown(socket_fd, SHUT_RDWR);	
}

/* handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_NOTICE, "Caught signal, exiting\n");		
		remove(FILE_PATH);
		close_all();		
		exit (EXIT_SUCCESS);
	}   
}
int main(int argc, char *argv[])
{
	int daemon_mode = 0;
	int rc;
	openlog(NULL, 0, LOG_USER);
	
	/*-------------------------------------------------------------------*/	
	/* Setting up signal handlers SIGINT and SIGTERM */
	if(signal(SIGINT, signal_handler) == SIG_ERR)
	{
		printf("Could not setup single handler for SIGINT\n");
		return -1;
	}										

	if(signal(SIGTERM, signal_handler) == SIG_ERR)
	{
		printf("Could not setup single handler for SIGTERM\n");
		return -1;
	}
	 
	/* Creating signal set ,and add signal SIGINT SIGTERM into created empty set*/ 
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM); 
	//check if daemon mode specified at cmd line  
	if(argc == 2)
	{
		if(strcmp("-d", argv[1]) == 0)
		{
			daemon_mode = 1;
		}	
	}
	/*-------------------------------------------------------------------*/	
	//create socket 
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	//perror("socket:");
	if(socket_fd == -1)
	{
		syslog(LOG_ERR, "Error Creating socket\n");
		return -1;
	}
	
	// Reuse address
	int reuse_addr = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(int)) == -1) 
	{
		printf("setsockopt error");
		syslog(LOG_ERR, "setsockopt");
		close(socket_fd);
		closelog();
		return -1;
	}
	
	/*-------------------------------------------------------------------*/	

	/* call getaddrinfo to get socket addresses info linkelist to use for bind()
	   it converts hostnames/IP addresses into a dynamically allocated linked list of struct addrinfo structures 
	   arguments - hostname- domain name ,service,hints, structure to store result into 
	   returns result into - addrinfo structures, each of which contains an Internet address linkelist that can be specified
	   */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;   
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;      

	if(getaddrinfo(NULL, PORT_NUMBER, &hints, &sockaddrinfo) != 0)
	{
		syslog(LOG_ERR, "Error getting address info\n");
		close(socket_fd);
		closelog();
		return -1;
	}
	/*-------------------------------------------------------------------*/	

	rc = bind(socket_fd, sockaddrinfo->ai_addr, sockaddrinfo->ai_addrlen);
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in binding\n");
		close(socket_fd);
		closelog();
		return -1;
	}

	freeaddrinfo(sockaddrinfo); // free sockaddrinfo linked list
		/*-------------------------------------------------------------------*/	

	//if daemon mode provided at the cmd-line fork daemon child 
	// and exectue remaining program as daemon 
	if(daemon_mode)
	{
		int pid = fork();

		if (pid < 0 )
			syslog(LOG_ERR, "Forking error\n");

		if(pid > 0)
		{
			syslog(LOG_DEBUG, "Daemon created\n");
			exit(EXIT_SUCCESS);
		}
		if(pid == 0)
		{
			/* setting up daemon */

			//set session id 
			int sid = setsid();
			if(sid == -1)
			{
				syslog(LOG_ERR, "Error in setting sid\n");
				exit(EXIT_FAILURE);
			}
			//change current directory to root 
			rc=chdir("/");			
			if(rc== -1)				
			{
				syslog(LOG_ERR, "chdir");
				close(socket_fd);
				exit(EXIT_FAILURE);
			}

			//redirect stdin, sdtout, stderror to /dev/null 
			open("/dev/null", O_RDWR);
			perror("open");
			dup(0);
			dup(0);		
		}
	}
	
	/*-------------------------------------------------------------------*/	
	rc = listen(socket_fd,BACKLOG); //listen for connections on a socket
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in listen\n");
		close(socket_fd);
		closelog();
		return -1;
	}
	
	/*-------------------------------------------------------------------*/	
	//datastructure for client socket address  
	struct sockaddr_in clientsockaddr;
	socklen_t addr_size;
	addr_size = sizeof(clientsockaddr);
	
	/*-------------------------------------------------------------------*/	
	
	rx_buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);   //buffer for rx data 	
	if(rx_buf==NULL)
	{
		syslog(LOG_ERR,"malloc for rx buffer failed\n");
	}
	
	
	//open in append or  creating file /var/tmp/aesdsocketdata file if it doesn’t exist.
	outputfile_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, 0744);
	perror("open ouputfile");
	if (outputfile_fd == -1)
	{
		syslog(LOG_ERR, "Error in opening file");
		free(rx_buf);
		//free(tx_buf);
		close_all();
	}
	
	/*-------------------------------------------------------------------*/	
	int total_rxbytes = 0; //total rx bytes 
	int buffer_realloc_count = 0; 
	while(1)
	{
		/* accept a connection on a socket, creates a new connected socket, and returns a new file descrtor */
		client_fd = accept(socket_fd, (struct sockaddr *)&clientsockaddr, &addr_size);
		if(client_fd == -1)
		{
			syslog(LOG_ERR, "Error accepting connection from client\n");
			return -1;
		}
		
		/* print accepted client IP address */
		char *client_ip_address = inet_ntoa(clientsockaddr.sin_addr);		
		syslog(LOG_DEBUG,"Accepted connection from %s", client_ip_address);
		int current_rxbytes = 0;
		
		
		/*recieve all data in buffer by realllcating buffer size*/	
		int buf_location =0; 	
		do
		{
			current_rxbytes = recv(client_fd, (rx_buf+buf_location), BUFFER_SIZE-1, 0); 
			if(current_rxbytes == -1) 
			{
				//perror("recv");
				syslog(LOG_ERR, "sigprocmask");
				close_all();
				exit(EXIT_FAILURE);
			} 
			else 
			{				
				total_rxbytes += current_rxbytes;
				buf_location  += current_rxbytes; 	
			
    			buffer_realloc_count++; //incrementing buffer size by * BUFFER_SIZE
				rx_buf = (char*)realloc(rx_buf,(buffer_realloc_count)*(BUFFER_SIZE)*(sizeof(char)));				
				//perror("realloc");
				if(rx_buf == NULL)
				{
					syslog(LOG_ERR, "Error while reallocating buffer");
					close_all();
					exit(EXIT_FAILURE);
				}

			}
		}while(strchr(rx_buf, '\n') == NULL);//Detect newline character - if ewline exists, break out of loop 
		rx_buf[buf_location] = '\0';
		
		
		/* Mask off signals while transferring data */
		rc = sigprocmask(SIG_BLOCK, &mask, NULL);
		if(rc!=0) 
		{
			perror("adding mask for signal");
			syslog(LOG_ERR, "sigprocmask");
			free(rx_buf);			 
			close_all();
		}
		/* write full contents of buffer to ouputfile */
		int written_bytes= write(outputfile_fd, rx_buf, buf_location); // Write to file
		//perror("write");
		if(written_bytes ==-1)
		{
			syslog(LOG_ERR, "Error while writing to o/p file");			
			close_all();
			exit(EXIT_FAILURE);
		}
		
		// unmask signals after transfer finished  - remove singnal set from mask set 
		rc = sigprocmask(SIG_UNBLOCK, &mask, NULL);	
		//perror("sigprocmask unmasking");
		if(rc!=0) 
		{
			syslog(LOG_ERR, "sigprocmask");
			free(rx_buf);
			close_all();		
			exit(EXIT_FAILURE);
		}
		
		
		/* now copy full of file content into tx_buffer and send onto socket */		
		
		tx_buf = (char *)malloc(sizeof(char) * total_rxbytes);	  
		if(tx_buf==NULL)
		{
			syslog(LOG_ERR,"malloc for rx buffer failed\n");
			close_all();
			exit(EXIT_FAILURE);
		}
		 		
		lseek(outputfile_fd, 0, SEEK_SET);//Set position of file pointer to start for reading
		
		//read contents from file into tx_buf 
		int total_read_bytes = read(outputfile_fd,tx_buf,total_rxbytes);
		if(total_read_bytes ==1)
		{
			syslog(LOG_ERR, "Error while reading data from outputfile\n");
			close_all();
			exit(EXIT_FAILURE);
		}
		
		 
		/*--------------------------------------------------------------------*/ 	
		//send contents onto socket 		 
		int total_txbytes = send(client_fd,tx_buf,total_read_bytes,0);
		if(total_txbytes==-1)
		{
			syslog(LOG_ERR, "Error sending data onto socket\n");			
			close_all();
			exit(EXIT_FAILURE);
		}			
		
		if(total_txbytes==total_rxbytes)
		{
			//printf("all contents written succefully\n");
			syslog(LOG_DEBUG,"all contents written succefully\n");
		}		
	}
	 
	exit(EXIT_SUCCESS);
}
