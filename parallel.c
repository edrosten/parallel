#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv)
{	
	int num, *procs, i, pid;
	char line_buf[32768];
	char line_buf2[32768];
	int saved_line = 0;

	
	if(argc != 2)
	{
		fprintf(stderr, "%s: Error: supply number of processes.\n", argv[0]);
		return 1;
	}

	num = atoi(argv[1]);	

	if(num < 2)
	{
		fprintf(stderr, "%s: Error: silly number of processes.\n", argv[0]);
		return 1;
	}

	procs = calloc(num, sizeof(int));

	for(;;)
	{
		//Check to see of procs is full
		for(i=0; i < num; i++)
			if(procs[i] == 0)
				goto not_full;

		//We're currently full, so wait
		pid = wait3(&i, 0, 0);

		//Find this slot
		for(i=0; i < num; i++)
			if(procs[i] == pid)
				break;

		if(i == num)
			fprintf(stderr, "b0rk!!\n");
		

		not_full:
		

		if(saved_line)
		{
			memcpy(line_buf, line_buf2, sizeof(line_buf));
			saved_line = 0;
		}
		else
			fgets(line_buf, sizeof(line_buf), stdin);

		if(feof(stdin))
			break;
		
		pid = fork();

		if(pid==0) //We are the child process
		{
			execlp("bash", "bash", "-c", line_buf, NULL);
			fprintf(stderr, "%s: exec failed: %s\n", argv[0], strerror(errno));
			return -1;
		}
		else if(pid == -1)
		{
			fprintf(stderr, "Fork failed with code %s. Sleeping for 1 second.\n", strerror(errno));
			memcpy(line_buf2, line_buf, sizeof(line_buf));
			sleep(1);
			saved_line = 1;
		}

		//Otherwise, insert this process in to the PID list.
		procs[i] = pid;
	}
	
	//Wait for children to become free
	while(wait3(&i, 0, 0) != -1);

	return 0;
}
