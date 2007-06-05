#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <list>

using namespace std;

#define VERBOSE(X) if(verbose) clog << argv[0] << ": " << X << endl
#define ERROR(X) do{cerr << argv[0] << ": " << X << endl;return 1;}while(0)

int main(int argc, char** argv)
{	
	int verbose=0;
	int start=1;

	if(argc > 1 && argv[1] == string("-v"))
	{
		verbose=1;
		start=2;
	}

	VERBOSE("Starting...");
	int pid;
	
	if(argc-start < 3)
	{
		cerr << argv[0] << ": Error: supply  [-v] dir machine #processes [ machine #processes [... ]]" << endl;
		return 1;
	}
	

	//This records the ssh arguments (ie machine name) for each slot
	vector<string> machine_list;

	string dir=argv[start];

	if(dir == ".")
	{
		while(getcwd(&dir[0], dir.size()) == 0)
		{
			if(errno == ERANGE)
				dir.resize(dir.size() * 2);
			else
				ERROR("Could not read current directory: " << strerror(errno));
		}
		
		dir.resize(strlen(dir.data()));
		VERBOSE("Working directory is " << dir);
	}


	for(int i=start+1; i < argc; i += 2)
	{
		string machine=argv[i];

		if(i+1 == argc)
			ERROR("Supply number of processes for " << machine);

		int num = atoi(argv[i+1]);	

		if(num <= 0)
		ERROR("Silly number of processes (" << num << ") for " << machine);
		
		//Create slots
		for(int n=0; n < num; n++)
			machine_list.push_back(machine);
	}

	if(machine_list.size() < 1)
		ERROR("No machines given.");


	//Open /dev/null for reading. This will be used as stdin for all spawned
	//processes so that they can't steal input to the main program.
	int devnull = open("/dev/null", O_RDONLY);
	if(devnull == -1)
		ERROR("error: could not open /dev/zero (" << strerror(errno) << ")" << endl
		     << "If this error occurs, then your machine is _very_ seriously b0rked.");
	
	//This holds all the free slots. It starts off full
	set<int> free_slots;
	for(unsigned int i=0; i < machine_list.size(); i++)
		free_slots.insert(i);
	

	//Record which running/finishing PIDs are in which slot
	map<int,int> pid_to_slot;

	//Which lines failed to fork
	list<string> failed_lines;
	
	for(;;)
	{
		
		if(free_slots.empty())
		{
			int tmp;
			//We're currently full, so wait for a process to finish
			pid = wait3(&tmp, 0, 0);

			//Insert the finished process back in to the list of available slots
			free_slots.insert(pid_to_slot[pid]);
			pid_to_slot.erase(pid);
		}
		
		//Get a line of input...
		string line;

		if(failed_lines.empty())
			getline(cin, line);
		else
		{
			line = failed_lines.front();
			failed_lines.pop_front();
		}

		VERBOSE("Input line -->" << line << "<--");

		if(cin.eof())
			break;
		
		//Get the first free slot and remove it from the pool.
		int a_free_slot = *free_slots.begin();
		free_slots.erase(a_free_slot);
		
		pid = fork();


		if(pid==0) //We are the child process
		{
			string m = machine_list[a_free_slot];
			
			//Change in to the working director. This is necessary since ssh loses this
			line = "cd " + dir + "&& " + line;
			
			//If a machine name is specified, then ssh to it, otherwise run the process
			//locally
			if(m != "")
				line = "ssh -n " + m + " '" + line + "'";

			VERBOSE("Executing (with bash -c) -->" << line << "<--");
			
			//Make stdin point to /dev/zero
			close(0);
			dup2(devnull, 0);
			
			execlp("bash", "bash", "-c", line.c_str(), NULL);

			ERROR("exec failed: " << strerror(errno));
			return -1;
		}
		else if(pid == -1)
		{
			clog << "Fork failed with error " << strerror(errno) << ". Sleeping for second." << endl;	
			//Reinsert the stuff
			free_slots.insert(a_free_slot);
			failed_lines.push_back(line);
			sleep(1);
		}
		else
		{
			//Otherwise, insert this process in to the PID list.
			pid_to_slot[pid] = a_free_slot;
		}

	}

	int tmp;
	//Wait for children to become free
	VERBOSE("EOF on stdin reached. Waiting for children.");
	while(wait3(&tmp, 0, 0) != -1);
	VERBOSE("Done");

	return 0;
}
