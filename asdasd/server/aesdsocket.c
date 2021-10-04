#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define PORT_NUMBER "9000"
#define BACKLOG_CONNECTIONS 5
#define MAXDATASIZE 100 // max number of bytes we can get at once
#define FILE_PATH "/var/tmp/aesdsocketdata"

/* File descriptor to write received data */
int socket_fd, client_fd, write_file_fd;
struct addrinfo hints, *sockaddrinfo;
char ipstr[INET6_ADDRSTRLEN];
uint8_t startdaemon = 0;

  
/* handler for SIGINT and SIGTERM */
static void signal_handler (int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
	{
		syslog(LOG_NOTICE, "Caught signal, exiting\n");
		close(write_file_fd);
		remove(FILE_PATH);
		shutdown(socket_fd, SHUT_RDWR);
    }
    
    closelog();
    exit (EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{

    openlog("aesdsocket.c", LOG_PID, LOG_USER);
    // Register signals with handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    int opt;
    while ((opt = getopt(argc, argv,"d")) != -1) {
        switch (opt) {
            case 'd' :
                startdaemon = 1;
                break;
            default:
                break;
        }
    }

    int reuse_addr =1;

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
	syslog(LOG_ERR, "Error in capturing socket file descriptor\n");
	return -1;
    }
 

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me


    if(getaddrinfo(NULL, PORT_NUMBER, &hints, &sockaddrinfo) != 0)
    {
		syslog(LOG_ERR, "Error in capturing address info\n");
		return -1;
    }

    if(bind(socket_fd, sockaddrinfo->ai_addr, sockaddrinfo->ai_addrlen) == -1)
    {
		syslog(LOG_ERR, "Error in binding\n");
		return -1;
    }

    freeaddrinfo(sockaddrinfo); // free the linked list

    if(startdaemon)
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
	    int sid = setsid();

            if(sid == -1)
	    {
	    	syslog(LOG_ERR, "Error in setting sid\n");
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
    if(listen(socket_fd,BACKLOG_CONNECTIONS) == -1)
    {
		syslog(LOG_ERR, "Error in listen\n");
		return -1;
    }

   
	struct sockaddr_in clientsockaddr;	
    socklen_t addr_size;
    addr_size = sizeof(clientsockaddr);

    // This variable contains total bytes transferred over all connections
    int check_tot = 0;
    while(1)
    {

	client_fd = accept(socket_fd, (struct sockaddr *)&clientsockaddr, &addr_size);
	
	if(client_fd == -1)
	{
	    syslog(LOG_ERR, "Error in retrieving client fd\n");
	    return -1;
	}

	
	char *IP = inet_ntoa(clientsockaddr.sin_addr);	
    syslog(LOG_DEBUG,"Accepted connection from %s", IP );
	int numbytes = 0;

	char *buf;

	buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

	write_file_fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, S_IRWXU);

	if (write_file_fd == -1)
	{
		syslog(LOG_ERR, "Error in opening file");
	}


	do
	{
	    numbytes = recv(client_fd, buf, MAXDATASIZE-1, 0);
	    if(numbytes == -1) 
		{
        	perror("recv");
        	return -1;
    	} 
		else 
		{
			check_tot +=numbytes;
			int wr;
			wr = write(write_file_fd, buf, numbytes);

			if(wr == -1)
			{
				syslog(LOG_ERR, "Error in writing to file");
			}

	    }
	}while(strchr(buf, '\n') == NULL);

	buf[check_tot] = '\0';

	// Set cursor to beginning of file for reading
	lseek(write_file_fd, 0, SEEK_SET);


	// buffer used for reading from file and sending through socket
	char * write_buf;
	write_buf = (char *)malloc(sizeof(char) * MAXDATASIZE);

	int send_bytes_check = 0;

	// Read and send bytes in batches/packets of MAXDATASIZE
	while(send_bytes_check < check_tot)
	{
	    // seek the cursor read after the prev size of read
	    lseek(write_file_fd, send_bytes_check, SEEK_SET);
	    int read_byte = read(write_file_fd,write_buf, MAXDATASIZE);

	    // Send the bytes that were just read from file
	    send_bytes_check += read_byte;
            if (send(client_fd, write_buf,read_byte, 0) == -1)
                perror("send");
	}

	close(client_fd);
	free(buf);
	free(write_buf);
	close(write_file_fd);
    }
}

