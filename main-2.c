#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "queue.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

struct job{
	int id;  
    char *command;
    char *status;        
    char *start_time;    
    char *stop_time;
};

int Number_of_Processor;
int current_Used_Processor = 0;
queue *jobs;
struct job ListOfAllJobs[100];

void show_jobs(int maximum_jobid){
	
	int first_time = 0;
	for(int i=0; i < maximum_jobid; i++){
		if( strcmp(ListOfAllJobs[i].status, "Waiting") == 0 || strcmp(ListOfAllJobs[i].status, "Running") == 0){
			if(first_time == 0){
				printf("jobid	Command		 		Status\n");
				first_time++;
			}
			printf("%d	%-30s %s\n", ListOfAllJobs[i].id,ListOfAllJobs[i].command,ListOfAllJobs[i].status);
		}

	}
}

void show_history(int maximum_jobid){
	
	int first_time = 0;
	for(int i=0; i < maximum_jobid; i++){
		if( strcmp(ListOfAllJobs[i].status, "Success") == 0 || strcmp(ListOfAllJobs[i].status, "Failed") == 0){
			if(first_time == 0){
				printf("jobid	Command		 		starttime	 	endtime			Status\n");
				first_time++;
			}
			printf("%d	%-30s %s %s %s\n", ListOfAllJobs[i].id,ListOfAllJobs[i].command,ListOfAllJobs[i].start_time,ListOfAllJobs[i].stop_time,ListOfAllJobs[i].status);
		}

	}
}

//This Function Exract data from 1D array 
//Command and Arguments are Passed by reference

void extratingArgumnets(char *buffer ,char **command, char ***Arguments){
	
	int size = 0;
	for(int i=0; buffer[i] != ' ' && buffer[i] != '\0'; i++){
		size++;
	}
	size++;
	(*command) = malloc(size * sizeof(char));
	
	int i;
	for(i=0; i< size-1; i++){
		(*command)[i] = buffer[i];	
	}
	(*command)[size-1] = '\0';
	
	int numberOfArguments = 2;
	for( int j = i; buffer[j] != '\0'; j++){
		if(buffer[j] == ' ')
			numberOfArguments++;
	}	
	int current_argument = 0;
	//Allocating 2D array
	(*Arguments) = malloc(numberOfArguments * sizeof(char *)); 
	for(int j = 0; j < numberOfArguments; j++)
	  (*Arguments)[j] = malloc(20 * sizeof(char));  // Allocate each row separately
	i = -1;
	
	while(current_argument < numberOfArguments){
		int j = i+1;
		int k;
		for(k = 0; buffer[j] != '\0' && buffer[j] != ' '; k++,j++){
			(*Arguments)[current_argument][k] =  buffer[j];
		}
		(*Arguments)[current_argument][k] = '\0';
		i = i+k+1;
		current_argument++;
	}
	(*Arguments)[numberOfArguments-1] = NULL;
	current_argument = 0;
}


char* get_current_time(){
	time_t mytime = time(NULL);
    char * time_str = ctime(&mytime);
    time_str[strlen(time_str)-1] = '\0';
    return time_str;
}

//Refernce: Get current time in c: https://stackoverflow.com/questions/5141960/get-the-current-time-in-c


int readline(char *string, int size)
{
    int i, c;
    for (i = 0; i < size - 1 && (c = getchar()) != '\n'; i++)
    {
        if (c == EOF)
            return -1;
        string[i] = c;
    }
    string[i] = '\0';
    return i;
}

void *execute_job(void *arg){
	current_Used_Processor++;
	int Job_Id = *((int *) arg);
	ListOfAllJobs[Job_Id].status = "Running";
//	sleep(2);
//	printf("Executing job # %d\n", Job_Id);
	ListOfAllJobs[Job_Id].start_time = get_current_time();
	int process_id = fork();
	int status_code;
	if(process_id == -1){
		perror("fork");
        exit(0);
	}
	else if(process_id == 0){
		char *command;
 		char **Arguments;
		extratingArgumnets(ListOfAllJobs[Job_Id].command,&command,&Arguments);
		
		/*Working For OutFile handling */ 
		char filename[10];
		sprintf(filename, "%d", Job_Id);
		strcat(filename, ".out");
		//Creating Out File
		int fd = open(filename, O_CREAT | O_APPEND | O_WRONLY, 0755);
		//Redirecting newly created out file to standerd STDOut
		dup2(fd, STDOUT_FILENO);
		
		/*Working For Error handling */ 
		char errorfilename[10];
		sprintf(errorfilename, "%d", Job_Id);
		strcat(errorfilename, ".err");
		//Creating Err File
		fd = open(errorfilename, O_CREAT | O_APPEND | O_WRONLY, 0755);
		//Redirecting newly created Error file to standerd ERR
		dup2(fd, STDERR_FILENO);
		
	
		
		status_code = execvp(command, Arguments);
        if(status_code == -1)
        {
        	fprintf(stderr, "Invalid Commands/ Arguments\n");
            exit(0);	
        }	
	} 
	else
	{
		//Parent Process
		wait(NULL);
		if(status_code == -1)
        {
        	ListOfAllJobs[Job_Id].status = "Failed";
        }else{
			ListOfAllJobs[Job_Id].status = "Success";
		}
		ListOfAllJobs[Job_Id].stop_time = get_current_time();
		current_Used_Processor--;
	}
	
}


//Redirecting output in c using dup: https://stackoverflow.com/questions/2605130/redirecting-exec-output-to-a-buffer-or-file 

void *job_scheduler(void *arg)
{
    while(1)
    {
        if ( jobs->count > 0 && current_Used_Processor < Number_of_Processor)
        {
        	
            int jobID = queue_delete(jobs);
            pthread_t Tid;
            
            int *arg = malloc(sizeof(*arg));
	        *arg = jobID;

            pthread_create(&Tid, NULL, execute_job, arg);
			pthread_detach(Tid);
        }
        sleep(1);
    }
    
	return NULL;
}


int main(int argc, char *argv[]){
		
	if(argc < 2){
		printf("Please Enter Number of Processor as Commmand Line Argument\n");
		exit(0);
	}
	
	Number_of_Processor = atoi(argv[1]);
	
	if(Number_of_Processor < 1 || Number_of_Processor > 8){
		printf("Please Enter Valid Number of Processor (1-8)\n");
		exit(0);
	}
	
	//Creating a queue of 100 size
	jobs = queue_init(100);
	
	pthread_t id; 
    pthread_create(&id, NULL,  job_scheduler, NULL);
	
	printf("Enter Commmand> ");
	//Read User input from main thread and create a new job accordingly
	char data[1000];
	char first_word[100];
	int job_id = 0;
	while(readline(data, 1000) != -1){
		
		int count =0;
		for(count =0; data[count] != ' ' &&  data[count] != '\0' ; count++ )
		{
			first_word[count] = data[count];
		}
		first_word[count] = '\0';
		
		if(strcmp(first_word , "submit") == 0){
			
			char command[1000];
			int count2 =0;
			count++;
			for(count ; data[count] != '\0' ; count++ )
			{
				command[count2] = data[count];
				count2++;
			}
			command[count2] = '\0';
			
			struct job j;
			j.id = job_id;
			j.command = (char*)malloc(strlen(command) * sizeof(char));
			strcpy(j.command ,command);
			j.status =  "Waiting";
			ListOfAllJobs[job_id] = j;
            queue_insert(jobs, job_id);
            printf("Job %d added to Queue.\n", job_id);
			job_id++;
			
		}
		else if(strcmp(first_word , "showjobs") == 0)
		{
			
			show_jobs(job_id);
			
		}else if(strcmp(first_word , "submithistory") == 0){
			
			show_history(job_id);
		}
		else{
			printf("Invalid Command Enter\n");
		}
		
		printf("Enter Commmand> "); 
	}
}
