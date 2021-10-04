/*
Author : Mehul Patel 
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
char *rx_buf;
char *tx_buf;

/* handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_NOTICE, "Caught signal, exiting\n");		
		remove(FILE_PATH);
		free(rx_buf);
		free(tx_buf);
		closelog();
		close(outputfile_fd);	
		close(client_fd);		
		shutdown(socket_fd, SHUT_RDWR);			
		exit (EXIT_SUCCESS);
	}   
}

int main(int argc, char *argv[])
{
	int daemon_mode = 0;
	int rc;
	openlog(NULL, 0, LOG_USER);

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

	//create socket 
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(socket_fd == -1)
	{
		syslog(LOG_ERR, "Error Creating socket\n");
		return -1;
	}

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
		return -1;
	}

	rc = bind(socket_fd, sockaddrinfo->ai_addr, sockaddrinfo->ai_addrlen);
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in binding\n");
		return -1;
	}

	freeaddrinfo(sockaddrinfo); // free sockaddrinfo linked list

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
			if (chdir("/") == -1)
			{
				syslog(LOG_ERR, "chdir");
				close(socket_fd);
				exit(EXIT_FAILURE);
			}

			//redirect stdin, sdtout, stderror to /dev/null 
			open("/dev/null", O_RDWR);
			dup(STDIN_FILENO);
			dup(STDOUT_FILENO);
			dup(STDERR_FILENO);

			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}
	}
	//listen for connections on a socket
	rc = listen(socket_fd,BACKLOG);
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in listen\n");
		return -1;
	}

	//datastructure for client socket address  
	struct sockaddr_in clientsockaddr;
	socklen_t addr_size;
	addr_size = sizeof(clientsockaddr);

	int total_rxbytes = 0; //total rx bytes 
	while(1)
	{
		//accept a connection on a socket, creates a new connected socket, and returns a new file descrtor 
		client_fd = accept(socket_fd, (struct sockaddr *)&clientsockaddr, &addr_size);

		if(client_fd == -1)
		{
			syslog(LOG_ERR, "Error accepting connection from client\n");
			return -1;
		}

		char *client_ip_address = inet_ntoa(clientsockaddr.sin_addr);		
		syslog(LOG_DEBUG,"Accepted connection from %s", client_ip_address);
		int current_rxbytes = 0;

		//open in append or  creating file /var/tmp/aesdsocketdata file if it doesn’t exist.
		outputfile_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, 0744);
		if (outputfile_fd == -1)
		{
			syslog(LOG_ERR, "Error in opening file");
		}

		//Mask off signals while transferring data 
		rc = sigprocmask(SIG_BLOCK, &mask, NULL);
		if(rc!=0) 
		{
			syslog(LOG_ERR, "sigprocmask");
			closelog();
			close(client_fd);
		}
		/*
		   recieving data in BUFFER_SIZE bytes packets  and writing into file 

		   packet is considered complete when a newline character is found in the input receive stream,
		   and each newline should result in an append to the /var/tmp/aesdsocketdata file.
		   */	
		//DMA for buffer for recieving data and writing		
		rx_buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);

		do
		{
			current_rxbytes = recv(client_fd, rx_buf, BUFFER_SIZE-1, 0);
			if(current_rxbytes == -1) 
			{
				//perror("recv");
				syslog(LOG_ERR, "sigprocmask");
				exit(EXIT_FAILURE);
			} 
			else 
			{
				total_rxbytes +=current_rxbytes;
				// Write to file
				int written_bytes= write(outputfile_fd, rx_buf, current_rxbytes);
				if(written_bytes == -1)
				{
					syslog(LOG_ERR, "Error in writing to file");
					exit(EXIT_FAILURE);
				}

			}
		}while(strchr(rx_buf, '\n') == NULL);//Detect newline character - if ewline exists, break out of loop 
		rx_buf[total_rxbytes] = '\0';

		/* 
		   Return the full content of /var/tmp/aesdsocketdata file to to the client 
		   as soon as the received data packet completed 			
		   */		
		//Set position of file pointer to start for reading
		lseek(outputfile_fd, 0, SEEK_SET);

		//DMA for buffer for reading data from file  and tx onto socket
		tx_buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
		memset(tx_buf, 0, BUFFER_SIZE);
		int total_txbytes = 0;
		//Read and send bytes in batches/packets of BUFFER_SIZE till all rx_bytes sent
		while(total_txbytes < total_rxbytes)
		{
			//move file pointer position after current total no of send bytes
			lseek(outputfile_fd, total_txbytes, SEEK_SET);
			memset(tx_buf, 0, BUFFER_SIZE);
			int read_byte = read(outputfile_fd,tx_buf, BUFFER_SIZE);

			/*
			   read BUFFER_SIZE bytes from remaining data bytes from file
			   and send onto scoket 
			   */
			total_txbytes += read_byte;
			rc = send(client_fd, tx_buf,read_byte, 0);
			if(rc== -1)
			{
				perror("send");
				syslog(LOG_ERR, "Error in sending data on socket");
			}

		}
		// unmask signals after transfer finished  - remove singnal set from mask set 
		rc = sigprocmask(SIG_UNBLOCK, &mask, NULL);	
		if(rc!=0) 
		{
			syslog(LOG_ERR, "sigprocmask");
			free(rx_buf);
			free(tx_buf);			
			closelog();
			close(client_fd);
			close(outputfile_fd);		
			exit(EXIT_FAILURE);
		}
		free(rx_buf);
		free(tx_buf);
		closelog();
		close(client_fd);
		close(outputfile_fd);		
	}
	shutdown(socket_fd, SHUT_RDWR);
	exit(EXIT_SUCCESS);
}

