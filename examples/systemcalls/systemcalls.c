/** 
	Author : Mehul
	completed - modified given assignment code 

	do_system - execute with system()
	do_exec - create child process usign fork, execv and exectue command in child - wait for child to complete in parent -  get exit status 
	do_exec_redirect -same like do_exec, redirect standard out to a file specified by outputfile, The rest of the behaviour is same as do_exec()

	refence source code :The Linux Programming Interface
	
	Chapter 24: Process Creation			 https://man7.org/tlpi/code/online/all_files_by_chapter.html#ch24
	Chapter 26: Monitoring Child Processes   https://man7.org/tlpi/code/online/all_files_by_chapter.html#ch26
*/ 
#include "systemcalls.h"


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>


/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
 */
bool do_system(const char *cmd)
{

	//Call the system() function with the command set in the cmd
	//return a boolean true if the system() call completed with success false if it returned a failure
	 
	int ret = system(cmd);
	if(ret == 0 && cmd==NULL)
		return false;
	else if (ret == -1 || ret ==127)
		return false;
	else		
		return true;
}

/**
 * @param count -The numbers of variables passed to the function. The variables are command to execute.
 *   followed by arguments to pass to the command
 *   Since exec() does not perform path expansion, the command to execute needs
 *   to be an absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in execv()
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() call, false if an error occurred, either in invocation of the 
 *   fork, waitpid, or execv() command, or if a non-zero return value was returned
 *   by the command issued in @param arguments with the specified arguments.
 */

bool do_exec(int count, ...)
{
	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
		command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	// this line is to avoid a compile warning before your implementation is complete
	// and may be removed
	command[count] = command[count];


	int return_Val;
	int child_status;

	pid_t pid;
	pid = fork();  
	//fork will create child duplicate like parent process  -it will exectue code below fork 
	//parent gets child's pid>0
	//and in child process pid = 0 

	//-ve value  : child could not be created 
	if (pid == -1)
	{
		va_end(args);
		return false;
	}
	//child created and exectue command usign execv() in child process
	else if(pid == 0)
	{	
		//if current process child is exectue 
		int ret = execv(command[0], command);

		//command[0] as the full path to the command to execute
		//using the remaining arguments as second argument to the execv() command
		if(ret == -1)
			exit(-1);
	}

	//parent wait for child 
	if(pid > 0)
	{		
		if (waitpid (pid, &child_status, 0) == -1) //wait for child 
		{
			return_Val = false; 
		}
		else
		{
			if (WIFEXITED(child_status))
			{
				if (WEXITSTATUS (child_status) == 0)
				{
					return_Val = true; 
				}
				else 
				{
					return_Val = false; 	
				}
			}
			else 
			{
				return_Val = false;
			}
		}
	}		

	va_end(args);

	return return_Val;
}

/**
 * @param outputfile - The full path to the file to write with command output.  
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
	int return_Val;
	int child_status;

	va_list args;
	va_start(args, count);
	char * command[count+1];
	int i;
	for(i=0; i<count; i++)
	{
		command[i] = va_arg(args, char *);
	}
	command[count] = NULL;
	// this line is to avoid a compile warning before your implementation is complete
	// and may be removed
	command[count] = command[count];


	//o/p redirection refernce : https://stackoverflow.com/a/13784315/1446624
	pid_t pid;

	//redirecting standard out to a file specified by outputfile
	int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	dup2(fd, 1);  //redirect stdout=> fd - file 


	pid = fork(); //fork will create child duplicate like parent process
	if(pid == -1) //child could not be created 
	{
		return_Val = 0;
	}
	else if(pid ==0) //if current process is child pid = 0 
	{
		//exectue command in child 
		int ret = execv(command[0],command);
		if(ret ==-1)
			exit(-1);
	}

	//execution in parent (parent pid>0) 
	// parent wait to get child status  
	if(pid > 0)
	{		
		if (waitpid (pid, &child_status, 0) == -1) //wait for child 
		{
			return_Val = false; 
		}
		else
		{
			if (WIFEXITED(child_status))
			{
				if (WEXITSTATUS (child_status) == 0) //returns true if the child terminated normally,
				{
					return_Val = true; 	
				}
				else 
				{
					return_Val = false; 	
				}
			}
			else 
			{
				return_Val = false;
			}
		}
	}		

	va_end(args);
	close(fd); //close file safely 
	return return_Val;
}
