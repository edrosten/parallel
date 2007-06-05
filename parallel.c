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

		//Otherwise, insert this process in to the PID list.
		procs[i] = pid;
	}
	
	//Wait for children to become free
	while(wait3(&i, 0, 0) != -1);

	return 0;
}
