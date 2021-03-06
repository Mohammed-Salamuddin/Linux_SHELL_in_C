#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "mr_header.h"
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MR_READLINE_BUFSIZE 1024
#define MR_TOK_BUFSIZE 64
#define MR_TOK_DELIM " \t\r\n\a"
#define MR_PROCESS_GROUP_SIZE 50
#define MR_HISTORY_SIZE 25

//Global variables
int last_index;
char infile[20];
char outfile[20];
char *alias_cmd[50]; //can story 50 cmds
char *long_cmd[50];
int alias_idx=0;
int is_in_redirection=0,is_out_redirection=0;

/*
  List of builtin commands.
 */
char *builtin_str[] = {
	"cd",
	"help",
	"exit",
	"history"
};
/*
    array of functions.which takes parameters too.
    can use switch too instead.
*/
int (*builtin_func[]) (char **) = {
	&mr_cd,
	&mr_help,
	&mr_exit,
	&mr_history
};

int mr_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/**
   @brief Bultin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */


struct history
{
	char* history; //do malloc
	char* cur_time;
	int pid;
	
}hist_list[MR_HISTORY_SIZE];
int h=0; //history index

int mr_history(char** args)
{
	char* ct = malloc(sizeof(char)*30);
	if(args[1]==NULL)
	{
		int j=0;	
		while(hist_list[j].history!=NULL)
		{
			strcpy(ct,hist_list[j].cur_time);
			ct[strlen(ct)-1] = '\0'; 
			printf("  %d  %d  %s     %s\n",j+1,hist_list[j].pid,ct,hist_list[j].history);
			j++;
		}
		return 1;
	}

	pid_t pid,pid1;
	int	pipe_fd[2];

	pipe(pipe_fd);

	//history is written into the pipe.
	//read from the pipe and do grep	
	if(args[1]!=NULL)
	{
		
		//hook the STDOUT to a pipe.
		//fork and call execlp(args[1],args[2],NULL)
		pid = fork();
       	if(pid==0)
        {
			close(pipe_fd[0]);
			if(dup2(pipe_fd[1], STDOUT_FILENO) < 0)
			{
				perror("mr error from history grep 1");
				exit(EXIT_FAILURE);
			}

			int j=0;	
			while(hist_list[j].history!=NULL)
			{
				printf("  %d  %s         %d  %s \n",j+1,hist_list[j].history,hist_list[j].pid,hist_list[j].cur_time);
				j++;
			}
			exit(0);
		}

		pid1 = fork();
		if(pid1 == 0)
		{
			close(pipe_fd[1]);
			if(dup2(pipe_fd[0], STDIN_FILENO) < 0)
			{
				perror("dup2");
				exit(EXIT_FAILURE);
			}

            if (execlp(args[1],args[1],args[2],NULL) == -1)
                perror("mr error in history grep 2");
            exit(EXIT_FAILURE); 
			
		}
		/**Parent closes the pipes and wait for children*/
		close(pipe_fd[0]);
		close(pipe_fd[1]);

		wait(0);
		wait(0);

	}

	return 1;
}


int mr_cd(char **args)
{
  	if (args[1] == NULL) 
		fprintf(stderr, "mr: expected argument to \"cd\"\n");
	else 
	{
    	if (chdir(args[1]) != 0)
      		perror("mr");
		//setenv("PWD",strcat(getenv("PWD"),"/"),1);
		//setenv("PWD",strcat(getenv("PWD"),args[1]),1);
	}
  return 1;
}

/*
   brief Builtin command: print help.
   param args List of args.  Not examined.
   return Always returns 1, to continue executing.
 */

int mr_help(char **args)
{
  int i;
  printf("---------------MR HELP-----------------\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < mr_num_builtins(); i++) {
    printf("      %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

int mr_exit(char **args)
{
  printf("Khuda hafiz...\n");
  return 0;
}

int mr_execute(char **args)
{
	hist_list[h-1].pid = getpid();

	//first we mapped full command to its alias eg cat 1=cat in
	//now we map each argument to its alias eg du -s= dus (should execute dus in)

	if (args[0] == NULL)
		return 1;// An empty command was entered.
	
	args = mr_substitute_with_alias(args);

	int i;
	for (i = 0; i < mr_num_builtins(); i++) 
		if (strcmp(args[0], builtin_str[i]) == 0)  //or send args[0] to the switch,it will match the case and call the respective function.
			return (*builtin_func[i])(args);

	return mr_just_exec(args);
}

int mr_just_exec(char **args)
{
	pid_t pid;
	int status;

	char* mr_env[] = {"USER=unknown", "PATH=/bin", NULL};
  	setenv("USER","salam_raihan",1);
  	setenv("PATH",".:/home/salam/.local/share/umake/bin:/home/salam/bin:/home/salam/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin",1);
  	setenv("SHELL","/home/salam/Desktop/USP_project_2/mr",1);

	pid = fork();
	if (pid == 0) 
	{
		if (execvp(args[0], args) == -1)
    		perror("mr execvp");
		/*note
		this is only executed when the exec fails,coz exec changes the whole child 
		to the passed program,therefore the rest of the code in the child is not executed.
		*/
		exit(EXIT_FAILURE);
	} 
	else if (pid < 0)
		perror("mr:Error forking");
	else 
	{
		hist_list[h-1].pid = pid;
		// Parent process
		do 
		{
			waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return 1;
}


/*
   	brief Split a line into tokens (very naively).
	param line The line.
   	return Null-terminated array of tokens.
 */


char **mr_split_line(char *line)
{
  int bufsize = MR_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "mr: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, MR_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += MR_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		//free(tokens_backup);
        fprintf(stderr, "mr: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, MR_TOK_DELIM);
  }
  tokens[position] = NULL; //Null-terminated array
  return tokens;
}

char *mr_read_line(void)
{
	int bufsize = MR_READLINE_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) 
	{
		fprintf(stderr, "mr: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) 
	{
		// Read a character
		c = getchar();

		if (c == EOF) 
			exit(EXIT_SUCCESS); 
		else if (c == '\n') 
		{
			buffer[position] = '\0';
			return buffer;
		} 
		else 
		{
			buffer[position] = c;
		}
		position++;

		// If we have exceeded the buffer, reallocate.
		if (position >= bufsize) 
		{
			bufsize += MR_READLINE_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) 
			{
				fprintf(stderr, "mr: allocation error\n");
				exit(EXIT_FAILURE);
			}
    	}
  	}
}

//execute the commands seperated by pipes.
int mr_execute_pipe(char **cmds,int pipe_count)
{
    char ** args;
	pid_t pid;
	int status;

	int pipefds[2*pipe_count]; 
    
	for(int k=0;k<pipe_count;k++)
	{
		if(pipe(pipefds + k*2)<0)
		{
            perror("couldn't pipe");
            exit(EXIT_FAILURE);			
		}
	}

	int i=0;
	int j=0;
	
	while(cmds[i]!=NULL)
	{	
		args = mr_split_line(cmds[i]);

		//do the alias here! done
		args = mr_substitute_with_alias(args);		

        pid = fork();
		if(pid < 0)
        {
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        else if(pid==0)
        {
			
			//if not last command
			if(cmds[i+1]!=NULL)
			{
				if(dup2(pipefds[j + 1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
			}
			//if not first command&& j!= 2*numPipes
			if(j!=0)  //middle command
			{
				if(dup2(pipefds[j-2], STDIN_FILENO) < 0)
                {
                    perror(" dup2");///j-2 0 j+1 1
                    exit(EXIT_FAILURE);

                }
			}
		
			// close all pipe-fds
			for(i = 0; i < 2 * pipe_count; i++)
			{
				close(pipefds[i]);
			}

            if (execvp(args[0], args) == -1)
                perror("mr execvp_of_pipe_1");
            exit(EXIT_FAILURE); 
        }
		hist_list[h-1].pid = pid;

        //free(args);
        i++;
		j+=2;
    }


	/**Parent closes the pipes and wait for children*/
	for(i = 0; i < 2 * pipe_count; i++)
	{
		close(pipefds[i]);
	}

	//only the parent gets here and waits for pipes+1(coz if 1 pipe then 2 cmds) children to finish
	for(i = 0; i < pipe_count + 1; i++)
		wait(&status);	

	//if anything goes worng,perror will take care.

	return 1;
}

int mr_execute_both(char**cmds,int pipe_count)
{
	printf("\ncount of pipes:%d",pipe_count);
    char ** args;
	pid_t pid;  
	int status;

	int pipefds[2*pipe_count]; 	
    
	for(int k=0;k<pipe_count;k++)
	{
		if(pipe(pipefds + k*2)<0)
		{
            perror("couldn't pipe");
            exit(EXIT_FAILURE);			
		}
	}

	int i=0;
	int j=0;
	while(cmds[i]!=NULL)  
	{	
		args = mr_split_line(cmds[i]);

		args = mr_substitute_with_alias(args);

        pid = fork();
		if(pid < 0)
        {
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        else if(pid==0)
        {
			
			if(i==0)  //1st command,check for infile and outfile for redirection
			{	
				int infile_fd = open(infile,O_RDONLY);

				if(dup2(infile_fd,STDIN_FILENO)<0)
				{
					perror("dup2");
                    exit(EXIT_FAILURE);
				}
			}
			//if not last command
			if(cmds[i+1]!=NULL)
			{
				if(dup2(pipefds[j + 1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
			}
			//if not first command&& j!= 2*numPipes
			if(j!=0)  //middle command
			{
				if(dup2(pipefds[j-2], STDIN_FILENO) < 0)
                {
                    perror(" dup2");///j-2 0 j+1 1
                    exit(EXIT_FAILURE);

                }
			}
			if(cmds[i+1]==NULL) //last command
			{
				int outfile_fd = open(outfile,O_RDWR|O_APPEND);
				
				if(dup2(outfile_fd,STDOUT_FILENO)<0)
				{	
					perror("dup2");
                    exit(EXIT_FAILURE);
				}
			}
		
			// close all pipe-fds
			for(i = 0; i < 2 * pipe_count; i++)
			{
				close(pipefds[i]);
			}

            if (execvp(args[0], args) == -1)
                perror("mr execvp_of_both_1");
            exit(EXIT_FAILURE); 

        }//childs ends
		hist_list[h-1].pid = pid;
        //free(args);
        i++;
		j+=2;
    }

	/**Parent closes the pipes and wait for children*/
	for(i = 0; i < 2 * pipe_count; i++)
	{
		close(pipefds[i]);
	}

	//only the parent gets here and waits for pipes+1(coz if 1 pipe then 2 cmds) children to finish
	for(i = 0; i < pipe_count + 1; i++)
		wait(&status);	

	//if anything goes worng,perror will take care.


	return 1;
}


//only redirection(<,>,<>),no pipe nothing
int mr_execute_redirection(char **cmds)
{
	//printf("\ncmds[0]:%s:\n",cmds[0]);
	//printf("in,out:%d,%d\n",is_in_redirection,is_out_redirection);
    char ** args;
    pid_t pid;

    args = mr_split_line(cmds[0]); //only cmds[0] exits,eg cat<in
	args = mr_substitute_with_alias(args);

    pid = fork();
    if(pid < 0)
    {
        perror("fork error");
        exit(EXIT_FAILURE);
    }
    else if(pid==0)
    {
		if(is_in_redirection && is_out_redirection) //  only cat<in>out  do cat>out<in
        {
            int infile_fd = open(infile,O_RDONLY);
            if(dup2(infile_fd,STDIN_FILENO)<0)
            {
					perror("dup2 infile");
                    exit(EXIT_FAILURE);
            }

            int outfile_fd = open(outfile,O_RDWR|O_APPEND);            
            if(dup2(outfile_fd,STDOUT_FILENO)<0)
            {
                perror("dup2 outfile");
                exit(EXIT_FAILURE);
            }
        } 
		else if(is_in_redirection)  //first cmd i.e only cat<in
        {
            int infile_fd = open(infile,O_RDONLY);
            if(dup2(infile_fd,STDIN_FILENO)<0)
            {
					perror("dup2 infile");
                    exit(EXIT_FAILURE);
            }
        }
        else if(is_out_redirection)  //last command i.e only cat>out
        {            
			
            int outfile_fd = open(outfile,O_RDWR|O_APPEND);
            
            if(dup2(outfile_fd,STDOUT_FILENO)<0)
            {
                perror("dup2 outfile");
                exit(EXIT_FAILURE);
            }
        }
  
		if (execvp(args[0], args) == -1)
            perror("mr execvp_of_redirections");
        exit(EXIT_FAILURE);         

    }//child ends
	hist_list[h-1].pid = pid;
    wait(0);

	is_in_redirection=0;
	is_out_redirection=0;
    return 1;
}

//cat|wc<in>out
char** mr_split_both(char *line)
{
    char **cmds;
    cmds = malloc(MR_PROCESS_GROUP_SIZE*sizeof(char*));

    int i=0,j=0;
    for(int k=0;k<strlen(line);k++)
    {
        if(line[k]=='>')
            i=k;
        if(line[k]=='<')
            j=k;
    }

	for(int i=0;i<MR_PROCESS_GROUP_SIZE;i++)
	{
		/* note
		strsep keeps removing from line so after this for loop its NULL
		uncomment the below print to see.
			?>cat|ls<in>out
			i=0,cmds:cat,line:ls<in>out
			i=1,cmds:ls,line:in>out
			i=2,cmds:in,line:out
			i=3,cmds:out,line:(null)
			i=4,cmds:(null),line:(null)
		*/
		cmds[i] = strsep(&line,"|><\n");
		
        //printf("\ni=%d,cmds:%s,line:%s\n",i,cmds[i],line);		
		if(cmds[i]==NULL)
		{			
			last_index = i-1;
			break;
		}

	}

	//if infile is after out file.  ie cat>out<in
    if(i<j)
    {
        strcpy(infile,cmds[last_index]);
	    strcpy(outfile,cmds[last_index-1]);
    }
    else
    {
        strcpy(infile,cmds[last_index-1]);
        strcpy(outfile,cmds[last_index]);
    }
    cmds[last_index] = NULL;
    cmds[last_index-1] = NULL;

	//pure cmds with redirections removed.
    return cmds;
}

char** mr_split_redirection(char* line)
{
    char** cmds;
    cmds = malloc(MR_PROCESS_GROUP_SIZE*sizeof(char*));

    int i=0,j=0,k;
    for(k=0;k<strlen(line);k++)
    {
		if(line[k]=='<')
		{
			i=k;
            is_in_redirection =1;
		}
        if(line[k]=='>')
		{
			j=k;
			is_out_redirection=1;
		}
        

    }

    for(int i=0;i<MR_PROCESS_GROUP_SIZE;i++)
	{
		cmds[i] = strsep(&line,"<>\n");
		if(cmds[i]==NULL)
		{
			last_index = i-1;
			break;
		}
	}

	//cat<in>out or cat>out<in
	if(is_in_redirection  && is_out_redirection)
	{
		if(i<j) //in came first i.e  cat<in>out
    	{
        	strcpy(infile,cmds[last_index-1]);
        	strcpy(outfile,cmds[last_index]);
   	 	}
    	else  //cat>out<in
    	{
			strcpy(outfile,cmds[last_index-1]);
			strcpy(infile,cmds[last_index]);	   	 	
   	 	}
   	 	cmds[last_index-1] = NULL;
	}		
	else if(is_in_redirection) //found >
	{
		strcpy(infile,cmds[last_index]);		
	}
	else if(is_out_redirection)
	{
		strcpy(outfile,cmds[last_index]);		
	}
	cmds[last_index] = NULL;

    //cmds[0] = cd and cmds[1] = filename
    return cmds;
}

//splits baised on pipe,returns list of commands which were speprared by pipes
char ** mr_split_pipe(char *line)
{
	char **cmds;
	cmds = malloc(MR_PROCESS_GROUP_SIZE*sizeof(char*));

	for(int i=0;i<MR_PROCESS_GROUP_SIZE;i++)
	{
		cmds[i] = strsep(&line,"|");
		if(cmds[i]==NULL)
		{
			last_index = i-1;
			break;
		}
	}
	
	return cmds;
}

int mr_is_redirection(char* line)
{
	for(int i=0;i<strlen(line);i++)
		if(line[i] == '<' || line[i] == '>')
			return 1;
	return 0;    
}

//if pipe present returns 1 else 0
//checks only for 1 pipe
int mr_is_pipe(char *line)
{	
	int pipe_count=0;
	for(int i=0;i<strlen(line);i++)
		if(line[i] == '|')
			pipe_count++;
	if(pipe_count!=0)
		return pipe_count;
	else
		return 0;
}

/*
does checking for
1. pipe
2. redirection
3. normal cmd
*/
int mr_grand(char *line)
{
	int is_redirection=0;
	int is_pipe_count=0;
	int is_both=0;

    char **args;    
    char **cmds;
    int status;

	is_redirection = mr_is_redirection(line);
	is_pipe_count = mr_is_pipe(line);

	//cat|wc<in>out
	if(is_redirection && is_pipe_count)
    {			
		cmds = mr_split_both(line);   //cmds will have pointers to pure commands like ls,cat and redirection is removed.
		cmds = mr_substitute_with_alias(cmds);
		status = mr_execute_both(cmds,is_pipe_count);
    }

    //checks if redirection exists eg cat<in
    else if(is_redirection == 1)
    {
        cmds = mr_split_redirection(line);
		cmds = mr_substitute_with_alias(cmds);
        status = mr_execute_redirection(cmds);        
    }

    //checks if only pipe exists eg ls|wc
	else if(is_pipe_count) 
	{
		cmds = mr_split_pipe(line);
		cmds = mr_substitute_with_alias(cmds);	
		status = mr_execute_pipe(cmds,is_pipe_count);
	}

    //pure cammand eg ls -l or wc or pwd
	else
	{
		line = mr_do_alias_maping(line);
		args = mr_split_line(line);
    	status = mr_execute(args); 
	}
 
	//status is always 1,on error perror will take care.
   return status;
}

void mr_history_helper(char* line)
{
	time_t mytime;
	struct tm * timeinfo;
	time(&mytime);
	timeinfo = localtime(&mytime);

	//see if the history table is full,size is 25.	
	if(h==MR_HISTORY_SIZE)   //h == 25  
	{
		for(int i=1;i<=h-1;i++)
		{			
			strcpy(hist_list[i-1].history,hist_list[i].history);
			strcpy(hist_list[i-1].cur_time,hist_list[i].cur_time);
			hist_list[i-1].pid = hist_list[i].pid;
		}
		
		//hist_list[h-1].history = NULL;   gives segv why?
		//hist_list[h-1].cur_time = NULL;
		hist_list[h-1].pid = 0;
		strcpy(hist_list[h-1].history,line);
		strcpy(hist_list[h-1].cur_time,asctime(timeinfo));
		return;
	}
	hist_list[h].history = malloc(MR_READLINE_BUFSIZE*sizeof(char));
	hist_list[h].cur_time = malloc(50*sizeof(char));

	//hist_list[h].history = line; //never do this!
	strcpy(hist_list[h].history,line);
	strcpy(hist_list[h].cur_time,asctime(timeinfo));
	h++;

	return;
}

char** mr_substitute_with_alias(char **cmds)
{
	int i=0;
	while(cmds[i]!=NULL)
	{
		strcpy(cmds[i],mr_do_alias_maping(cmds[i]));
		i++;
	}
	return cmds;
}

char * mr_do_alias_maping(char *line)
{
	char *l = malloc(sizeof(char)*MR_READLINE_BUFSIZE);

	int i=0;
	while(i<alias_idx)
	{
        //printf("\n%s:\n",alias_cmd[i]);
		if(strcmp(alias_cmd[i],line)==0)
		{
			strcpy(l,long_cmd[i]);
			return l;
		}
		i++;
	}
	return line;
}

int mr_is_source_cmd(char *line)
{
	if(strcmp(line,"source aliasrc.")==0)
	{
		mr_sourcing(); 
		return 1;
	}	
	return 0;
}

//creates alias to original cmds mappings.uses the aliasrc. file
void mr_sourcing()
{
    FILE *fp;
	fp = fopen("aliasrc.", "r");
    //printf("inside alias function:%s:\n",line);  

    alias_idx=0;
	char c;
	int i=0;

	for(int j=0;j<50;j++)
	{
		alias_cmd[j] = malloc(sizeof(char)*20);
		long_cmd[j] = malloc(sizeof(char)*30);
	}
	char *buf1 = malloc(20*sizeof(char));
	char *buf2 = malloc(30*sizeof(char));

    c = fgetc(fp);

	while(!feof(fp))
	{
        while(c != ' ')
            c = fgetc(fp);
        
		i=0;
		c = fgetc(fp);
        //printf("\nfirst char of alias:%c/",c);
		while(c != '=')
		{
			buf1[i++] = c;
			c = fgetc(fp);
		}
		buf1[i] = '\0';
        //printf("cut alias:%s\n",buf1);

		c = fgetc(fp);
        //printf("\nfirst char of original:%c/",c);
		i=0;        
		while(c != '\n')
		{
			buf2[i++] = c;
			c = fgetc(fp);
		}
        c = fgetc(fp); //this is for skipping \n
		buf2[i] = '\0';
        //printf("cut original cmd:%s\n",buf2);

		strcpy(alias_cmd[alias_idx],buf1);
		strcpy(long_cmd[alias_idx],buf2);
		alias_idx++;
	}
    fclose(fp);

}

char *mr_custom(char *line)
{
	char** cmds;
	char* l = malloc(sizeof(char)*MR_READLINE_BUFSIZE);
	
	strcpy(l,line);
	cmds = mr_split_line(l);
	
	if(strcmp(line,"ps -z")==0)
		strcpy(line,"ps -all|grep defunct");

	else if(strcmp(cmds[0],"deepsearch")==0 || strcmp(cmds[0],"ds")==0)	
	{
		if(cmds[1]!=NULL)  //only ds gives segv
		{
			strcpy(line,"find . -type f -print | xargs grep ");
			strcat(line,cmds[1]);
		}
	}

	return line;
}

void mr_shell_start()
{
    char *line;  	//stores the cmd eg: ls -l
	line = malloc(sizeof(char)*MR_READLINE_BUFSIZE);
    int status;
	int i;
	char *next_line;
	
	printf("\n---------------welcome to MR_SHELL mr %s !-----------------\n\n",getenv("USER"));
	printf("Lets Begin\n");

    do 
    {
		int end_idx=0;
	    char* buf;
		
		buf = readline("\n?>");
    	if (strlen(buf) != 0) 
		{
			strcpy(line, buf);
			
		L1:	end_idx = strlen(line)-1;
			if(line[end_idx]=='\\')
			{
				next_line = readline(">");				
				line[strlen(line)-1] = '\0';  //remove \ and then combine.
				strcat(line,next_line);				
				goto L1;
			}
			
        	add_history(line);        		
		}
		else
			continue;

		mr_history_helper(line);

		if(mr_is_source_cmd(line))  //if source aliasrc. calls sourcing(),which will store maping globally. 
			continue;               //since its a source aliasrc. cmd no need to execute.
		
		line = mr_custom(line);
		status = mr_grand(line);
		
    } while (status);
}

int main(int argc,char **argv)
{
	//printf("mains pid %d\n",getpid());
	mr_sourcing();
    mr_shell_start();
    return EXIT_SUCCESS;
}