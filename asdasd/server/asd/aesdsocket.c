#include <stdio.h>
#include <stdlib.h>
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
#define BACKLOG 5
#define BUFFER_SIZE 100 // max number of bytes we can get at once
#define FILE_PATH "/var/tmp/aesdsocketdata"

int socket_fd; 
int outputfile_fd;
char *rx_buf;
char *tx_buf; 

/*signal handler for SIGINT and SIGTERM signals */
static void signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
		//printf("Caught signal, exiting\n");
		free(rxbuf);
		free(txbuf);
		syslog(LOG_INFO,"Caught signal, exiting\n");	
		remove(FILE_PATH);
		close(outputfile_fd);
		closelog();
		shutdown(socket_fd, SHUT_RDWR);	
		exit (EXIT_SUCCESS);
	}	
}

int main(int argc, char *argv[])
{	
	int client_fd; 
	
	struct addrinfo hints;
	struct addrinfo *sockaddrinfo;
	struct sockaddr_in clientsockaddr;
	socklen_t addr_size = sizeof(struct sockaddr);

	sigset_t mask;
	
	int daemon_mode = 0;
	
	int rc;
	
	if(argc == 2)
	{
		if(strcmp("-d", argv[1]) == 0)
		{
			startdaemon = 1;
		}	
	}
	 
	//for logging  
	openlog(NULL, 0, LOG_USER);
	
	/* Set signal handler for SIGINT  and SITERM */ 
	if(signal(SIGINT, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR, "Error setting singnal  SIGINT\n");
		exit(EXIT_FAILURE);
	}

	// Set signal handler for SIGTERM
	if(signal(SIGTERM, signal_handler) == SIG_ERR)
	{
		syslog(LOG_ERR, "Error setting singnal SIGTERM\n");
		exit(EXIT_FAILURE);
	}
	
	
	/* Creating signal set ,and add signal SIGINT SIGTERM into created empty set*/ 
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	 
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	//perror("socket");
	if(socket_fd == -1)
	{
		syslog(LOG_ERR, "Error in capturing socket file descriptor\n");
		return -1;
	}

	/* call getaddrinfo to get socket addresses info linkelist to use for bind()
	   it converts hostnames/IP addresses into a dynamically allocated linked list of struct addrinfo structures 
	   arguments - hostname- domain name ,service,hints, structure to store result into 
	   returns result into - addrinfo structures, each of which contains an Internet address linkelist that can be specified
	   */		 
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


	rc = getaddrinfo(NULL, PORT_NUMBER, &hints, &sockaddrinfo);
	if( rc != 0)
	{
		syslog(LOG_ERR, "Error in getting address info\n");
		exit(EXIT_FAILURE);
	}

	rc = bind(socket_fd, sockaddrinfo->ai_addr, sockaddrinfo->ai_addrlen);
	//perror("bind");
	if( rc == -1)
	{
		syslog(LOG_ERR, "Error in binding\n");
		exit(EXIT_FAILURE);
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
			syslog(LOG_DEBUG, "Daemon child created\n");
			exit(EXIT_SUCCESS);
		}
		if(pid == 0)
		{
			int sid = setsid();

			if(sid == -1)
			{
				syslog(LOG_ERR, "Error setting sid\n");
				exit(EXIT_FAILURE);
			}

			if (chdir("/") == -1)
			{
				syslog(LOG_ERR, "chdir");
				close(socket_fd);
				exit(EXIT_FAILURE);
			}

			open("/dev/null", O_RDWR);
			dup(STDIN_FILENO);
			dup(STDOUT_FILENO);
			dup(STDERR_FILENO);

			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}
	}
	rc = listen(socket_fd,BACKLOG);
	//perror("listen");
	if( rc == -1)
	{
		syslog(LOG_ERR,"Error in listen\n");
		return -1;
	}

	// This variable contains total bytes transferred over all connections
	int total_rxbytes = 0;
	
	outputfile_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, 0744);
	if (outputfile_fd == -1)
	{
		syslog(LOG_ERR, "Error in opening file");
	}
	
	
	while(1)
	{
		client_fd = accept(socket_fd, (struct sockaddr *)&clientsockaddr, &addr_size);
		//perror("accept");
		if(client_fd == -1)
		{
			syslog(LOG_ERR, "Error in retrieving client fd\n");
			return -1;
		}
		char *client_IP_addrress = inet_ntoa(clientsockaddr.sin_addr);
		syslog(LOG_DEBUG,"Accepted connection from %s", client_IP_addrress);
		
		/* add signal set for masking while transfer in process */
		rc = sigprocmask(SIG_BLOCK, &mask, NULL)
		//perror("sigprocmask");
		if (rc ==-1) 
		{
			syslog(LOG_ERR, "sigprocmask");			
			exit(EXIT_FAILURE);
		}
		
		/*
	    recieving data in BUFFER_SIZE bytes packets  and writing into file 

	    packet is considered complete when a newline character is found in the input receive stream,
	    and each newline should result in an append to the /var/tmp/aesdsocketdata file.
	    */
		//DMA for buffer for recieving data and writing  
		rx_buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
		if(rx_buff == NULL)
		{
			printf("malloc failed");
			exit(EXIT_FAILURE);
		}
		
		#if 1
		do
		{
			int current_rxbytes =0;
			current_rxbytes = recv(client_fd, rx_buf, BUFFER_SIZE-1, 0);
			perror("recv");
			if(current_rxbytes == -1) 
			{
				perror("recv");
				syslog(LOG_ERR, "Error in receiving data");
				exit(EXIT_FAILURE);
			} 
			else 
			{
				total_rxbytes +=current_rxbytes;
				int wr;
				wr = write(outputfile_fd, rx_buf, current_rxbytes);
				perror("write");
				if(wr == -1)
				{
					syslog(LOG_ERR, "Error in writing to file");
				}

			}
		}while(strchr(rx_buf, '\n') == NULL); //using newline to separate data packets received.
		rx_buf[total_rxbytes] = '\0';
		#endif 
		
		#if 0
		while(1)
		{
			int current_rxbytes =0;
			current_rxbytes = recv(client_fd, rx_buf, BUFFER_SIZE-1, 0);
			if(current_rxbytes == 0)
			{
				break;
			}
			else if(current_rxbytes ==-1)
			{
				syslog(LOG_ERR, "Error in receiving data");
				exit(EXIT_FAILURE);
			}
			else 
			{
				total_rxbytes +=current_rxbytes;
				int wr;
				wr = write(outputfile_fd, rx_buf, current_rxbytes);
				perror("write");
				if(wr == -1)
				{
					syslog(LOG_ERR, "Error in writing to file");
				}
			}
			
			if(strchr(rx_buf, '\n') == NULL)
			{
				break;
			}	
			
		}
		#endif
		
		/* 
		   Return the full content of /var/tmp/aesdsocketdata file to to the client 
		   as soon as the received data packet completed 			
		*/
		lseek(outputfile_fd, 0, SEEK_SET);	// Set file position to beginning of file for reading


		//DMA for buffer for reading data from file  and tx onto socket 
		tx_buf = (char *)malloc(sizeof(char) * BUFFER_SIZE);
		int total_txbytes = 0;

		//Read and send bytes in batches/packets of BUFFER_SIZE till all rx_bytes sent
		while(total_txbytes < total_rxbytes)  
		{				
			//move file pointer position after current total no of send bytes
			lseek(outputfile_fd, total_txbytes, SEEK_SET); 
			
			/*
			   read BUFFER_SIZE bytes from remaining data bytes from file
			   and send onto scoket 
			*/
			int read_byte = read(outputfile_fd,tx_buf, BUFFER_SIZE);
			total_txbytes += read_byte;
			if (send(client_fd, tx_buf,read_byte, 0) == -1)
				perror("send");
		}

		rc = sigprocmask(SIG_UNBLOCK, &mask, NULL);
		//remove singnal set - unmask signals after transfer finished  
		if(rc!=0) 
		{
			syslog(LOG_ERR, "sigprocmask");
			close(client_fd);
			close(outputfile_fd); 
			closelog();
			exit(EXIT_FAILURE);
		}		
		free(rx_buf);
		free(tx_buf);
	}
	
	
	shutdown(socket_fd, SHUT_RDWR);
	close(client_fd);
	close(outputfile_fd);
	closelog();	
	exit(EXIT_SUCCESS);
}

