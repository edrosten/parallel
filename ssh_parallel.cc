#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <list>

using namespace std;

#define VERBOSE(X) if(verbose) clog << argv[0] << ": " << X << endl
#define ERROR(X) do{cerr << argv[0] << ": Error: " << X << endl;return 1;}while(0)
#define NONFATALERROR(X) do{cerr << argv[0] << ": Error: " << X << endl;}while(0)
#define WARNING(X) do{cerr << argv[0] << ": Warning: " << X << endl;}while(0)

double gettimeofday()
{
	struct  timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec  + tv.tv_usec * 1e-6;
}



int main(int argc, char** argv)
{	
	int verbose=0;
	int start=1;
	unsigned int sleep_timeout=10;
	string nice="";
	
	ostringstream usage;
	usage << "[-n NICE] [-s TIME] [-v] [-h] dir machine #processes [ machine #processes [... ]]" << endl;


	{
		int error=0;
		int c;
		opterr=0;
		while((c=getopt(argc, argv, "hvn:s:")) != -1)
			switch(c)
			{
				case 'v':
					verbose=1;
					break;

				case 'h':
					cout << "Usage: " << argv[0] << " " << usage.str();
					cout << endl;
					return 0;
					break;

				case 's':
					{
						sleep_timeout=0;
						istringstream st(optarg);
						st >> sleep_timeout;

						if(sleep_timeout <= 0)
							ERROR("timeout must ba a positive integer. " << optarg << " is not valid.");
					}
				case 'n':
						nice = optarg;
					break;

				case '?':
					error=1;
					if(optopt == 'n')
						ERROR("-n requires an argument.");
					else
						ERROR("Error: Unknown option -" << (char)optopt);
					break;
				default:
					abort();
			}

		start=optind;
		if(error != 0)
			return 1;
	}

	
	VERBOSE("Starting...");
	
	if(argc-start < 3)
	{
		cerr << argv[0] << ": Error: " << usage.str();
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
	//Remember, fork() can not return the same PID twice, unless a wait() has
	//returned on that pid.
	map<pid_t,int> pid_to_slot;
	map<int, string> pid_to_line;

	//Which lines failed to fork
	list<string> failed_lines;

	//Record which slots have gone bad, along with their time.
	//This allows dead machines to be reinstated after a certain timeout
	//since there may be a temporary network error. If one machine has multiple
	//entries, then each slot will have to go bad, before the machine is discounted.
	//Map sorts in ascending order on the key.
	map<double, int> bad_slots;

	bool cin_ok = 1;
	
	for(;;)
	{
		//If there is no available input, or no free slots, then 
		//wait for a process to finish.
		while(free_slots.empty() || (!cin_ok && failed_lines.empty()))
		{
			//Check bad machines, and reinstate ones which have passed the timeout.
			double now = gettimeofday();

			if(!bad_slots.empty() && (now - sleep_timeout > bad_slots.begin()->first))
			{
				//Reinstate the machine
				int slot = bad_slots.begin()->second;
				VERBOSE("Reinstating " << slot << ": " << machine_list[slot]);
				free_slots.insert(slot);
				bad_slots.erase(bad_slots.begin());
			}

			//We're currently full, or there are no lines to process
			//so wait for a process to finish
			int status;
			pid_t pid = wait(&status);
			
			if(pid != -1)
			{
				//Check to see what happened
				int fail=0;

				if(WIFEXITED(status))
				{
					int exits = WEXITSTATUS(status);
					//SSH exits with 255 if it fails. 
					//ssh_parallel exits with 254 if exec fails
					//the remote script exits with 253 if "cd" fails.
					if(exits == 255 || exits == 254 || exits == 253)
					{	
						//If SSH failes, record the bad line, and the 
						//slot (ie machine) that failed.
						//Do NOT add the bad slot to the list of free slots.
						fail=1;

						if(exits == 255)
							WARNING("ssh to " << machine_list[pid_to_slot[pid]] << " failed.");
						else if(exits == 254)
						{
							WARNING("exec failed. Sleeping.");
							sleep(1);
						}
						else if(exits == 253)
							WARNING("directory change to " << dir << " on " << machine_list[pid_to_slot[pid]] << " failed.");

						VERBOSE("Marking " << pid_to_slot[pid] << "  as bad.");
						VERBOSE("Failed line was --->" << pid_to_line[pid] << "<---");
					}
				}
				else
				{
					//Insert the finished process back in to the list of available slots.
					//I don't know what to do in this case.
					WARNING("ssh terminated with signal" << WTERMSIG(signal));
					fail=0;
				}

				if(fail)
				{
					failed_lines.push_back(pid_to_line[pid]);
					bad_slots[gettimeofday()] = pid_to_slot[pid];
				}
				else
					free_slots.insert(pid_to_slot[pid]);


				//The pid has died, so remove it from the pid table.
				pid_to_slot.erase(pid);
				pid_to_line.erase(pid);
			}
			else
			{
				//If we've got here, then there are no processes to wait for.

				//If there are no processes to wait for, and no pending input, then 
				//we're done, so we can quit.

				if(!cin_ok && failed_lines.empty())
					exit(0);

				//if we've got here then there are no processes to wait for, but there is pending
				//input. That means (according to the loop condition) free_slots must be empty.
				//That means that all machines have gone bad, so sleep.
				WARNING("No live machines available. Sleeping.");
				sleep(sleep_timeout);
			}
		}

		
		//Get a line of input...
		string line;

		if(failed_lines.empty())
		{
			if(!cin_ok)
				continue;

			getline(cin, line);
			if(cin.eof())
			{
				cin_ok=false;
				continue;
			}
		}
		else
		{
			line = failed_lines.front();
			failed_lines.pop_front();
		}

		VERBOSE("Input line -->" << line << "<--");

		//Get the first free slot and remove it from the pool.
		int a_free_slot = *free_slots.begin();
		free_slots.erase(a_free_slot);
		
		pid_t pid = fork();

		if(pid==0) //We are the child process
		{
			string m = machine_list[a_free_slot];

			
			//Change in to the working director. This is necessary since ssh loses this
			line = "cd " + dir + " || exit 253 && " + line;
			
			//Alter the niceness if ncessary
			if(nice != "")
				line = "renice " + nice + " $$ > /dev/null && " + line;


			//If a machine name is specified, then ssh to it, otherwise run the process
			//locally
			if(m != "")
				line = "ssh -n " + m + " '" + line + "'";

			VERBOSE("Executing (with bash -c) -->" << line << "<--");
			
			//Make stdin point to /dev/zero
			close(0);
			dup2(devnull, 0);
			

			execlp("bash", "bash", "-c", line.c_str(), NULL);

			NONFATALERROR("exec failed: " << strerror(errno));
			return 254;
		}
		else if(pid == -1)
		{
			WARNING("Fork failed with error " << strerror(errno) << ". Sleeping for second.");
			//Reinsert the stuff
			free_slots.insert(a_free_slot);
			failed_lines.push_back(line);
			sleep(1);
		}
		else
		{
			//Otherwise, insert this process in to the PID list.
			pid_to_slot[pid] = a_free_slot;
			pid_to_line[pid] = line;
		}

	}

	int tmp;
	//Wait for children to become free
	VERBOSE("EOF on stdin reached. Waiting for children.");
	while(wait3(&tmp, 0, 0) != -1);
	VERBOSE("Done");

	return 0;
}
