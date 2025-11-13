/* 3000shell.c */
/* v2 Sept. 15, 2019 */
/* v1 Sept. 24, 2017 */
/* based off of csimpleshell.c, Enrico Franchi © 2005
      https://web.archive.org/web/20170223203852/
      http://rik0.altervista.org/snippets/csimpleshell.html */
/* Original under "BSD" license */
/* This version is under GPLv3, copyright 2017, 2019 Anil Somayaji */
/* You really shouldn't be incorporating parts of this in any other code,
   it is meant for teaching, not production */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>


#define BUFFER_SIZE (1<<16)
#define ARR_SIZE (1<<16)
#define COMM_SIZE 32
//#define DEBUG_TREESH

const char *proc_prefix = "/proc";


/* Extras for Assignment 2 Begin */
/* BRANCH_NUM is how many children there are when grow() works */
#define BRANCH_NUM 2
/* command_count is the counter of how many commands the shell has executed thus far */
static int command_count = 0;
/* this is where the parent treeSh keeps the output ends of pipes to "tree" children */
static int children_feeds_fd[BRANCH_NUM];
/* this is where the parent keeps track of "tree" children ids */
static int children_pids[BRANCH_NUM];
/* Extras for Assignment 2 End */


/* Extras for Assignment 3 Begin */

/* This will help the process to know at what level of the tree it is running */
static int level_count = 0;

#define MAX_BONSAI_SIZE 10


typedef struct shared {
  pthread_mutex_t delegate_mutex;
  sem_t grow_token;
  sem_t delegator_token;
  
  int delegate_idx;
  
  int queue[MAX_BONSAI_SIZE];
  char buffer[BUFFER_SIZE];

} shared;

static shared *delegation = NULL;

/* This function get's a PID of a process and a buffer to return the command.
 * If the PID correspond to that of the current delegate_idx,
 * then the buffer is filled with the command to execute _and_
 * delegate_idx is updated to the next possible delegate.
 * Otherwise, empty string is returned in the buffer.
 * The return value of the function is "0" if pid was correct and all went wel
 * It is 1, if the PID was not correct, but otherwise all went well
 * It is -1, if something happened that was not according to plan
 */
int get_delegated(int pid, char *buffer) {
  int current_delegate;
  int return_value = -1;
  pthread_mutex_lock(&(delegation->delegate_mutex));

  if ((delegation->delegate_idx < 0) || (strlen(delegation->buffer)==0) ) {
    return_value = -1;
  } else {

#ifdef DEBUG_TREESH
    fprintf(stderr,"I %d trying to find myself in the list of delegates using pid=%d\n",getpid(),pid);
#endif
    
    if (delegation->queue[delegation->delegate_idx]==pid) {
      strncpy(buffer, &(delegation->buffer[0]), BUFFER_SIZE);
      delegation->buffer[0]='\0';
      return_value = 0;
      
      /* Release the delegator */
      sem_post(&delegation->delegator_token);
      
      /* shift the delegate_idx */
      current_delegate = (delegation->delegate_idx+1) % MAX_BONSAI_SIZE;
      while ((delegation->queue[current_delegate]==-1) && (current_delegate != delegation->delegate_idx)) {
	current_delegate = (current_delegate +1) % MAX_BONSAI_SIZE;
      }
      delegation->delegate_idx = current_delegate;

#ifdef DEBUG_TREESH
      fprintf(stderr,"I %d found! myself in the list of delegates using pid=%d, and got a command: %s\n",getpid(),pid,buffer);
#endif
      
    } else {
      return_value = 1;    
    }
  } 
  
  pthread_mutex_unlock(&(delegation->delegate_mutex));  
  return return_value; // this line should never be reached
}

/* This function stores a delegated command
 * Return value 0 indicates that the command was read by a "deputy"
 * Return value of -1 indicates that delegation has failed
 */
int post_delegated(char *buffer) {

#ifdef DEBUG_TREESH
  fprintf(stderr,"I %d was asked to post the following command: %s\n",getpid(),buffer);
#endif
  
  pthread_mutex_lock(&(delegation->delegate_mutex));
  if ((delegation->delegate_idx<0) || (strlen(delegation->buffer)>0)) { // nobody to delegate to

#ifdef DEBUG_TREESH
    fprintf(stderr,"I am %d: Don't have a delegate: delegate id=%d\n",getpid(),delegation->delegate_idx);
#endif
    
    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return -1;
  } else {
    strncpy(&(delegation->buffer[0]),buffer, BUFFER_SIZE);

#ifdef DEBUG_TREESH
    fprintf(stderr,"I am %d: copied the command: %s\n",getpid(),delegation->buffer);
#endif
    
    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return 0;
  }
}

/* This function adds a new PID to the delegation queue
 * The function makes sure that PIDs appear in an increasing order
 * It also makes sure that the delegate_idx is updated so no "reversion" occurs.
 */
int insert_delegate(int pid) {
  int i,j;
  int temp_pid;
  pthread_mutex_lock(&(delegation->delegate_mutex));

  // find the insertion index
  /* Assignment-3 has to do something here */
  // insert the new PID into the list (shift the tail)
  /* Assignment-3 has to do something here */  
  // handle a special case of delegate_idx initialisation, because delegate_idx starts from being -1
  /* Assignment-3 has to do something here */
  
/* #ifdef DEBUG_TREESH */
/*   /\* fprintf(stderr,"Insertion called by %d: ",getpid()); *\/ */
/*   /\* for(i=0; i<MAX_BONSAI_SIZE; i++) *\/ */
/*   /\*   fprintf(stderr, "%d ",delegation->queue[i]); *\/ */
/*   /\* fprintf(stderr,"\n"); *\/ */
/*   /\* fflush(stderr); *\/ */
/* #endif */
  
  /* Assignment-3 has to do something here */
  return 0;
}

/* This function removes a PID from the delegation queue
 * The function makes sure that the delegate_idx remains valid and nor "jump-over" occurs
 */
int delete_delegate(int pid) {
  int i,j;
  int return_value = -1;
  pthread_mutex_lock(&(delegation->delegate_mutex));

  for(i=0;i<MAX_BONSAI_SIZE;i++) {
    if(delegation->queue[i]==pid)
      break;
  }

  if (i==MAX_BONSAI_SIZE) {
    return_value = 1; // no such PID
  } else {
    // shift the queue
    for(j=i;j<MAX_BONSAI_SIZE-1;j++) {
      delegation->queue[j]=delegation->queue[j+1];
    }
    delegation->queue[MAX_BONSAI_SIZE-1]=-1;
    if (i<delegation->delegate_idx)
      delegation->delegate_idx = delegation->delegate_idx-1;
    if (delegation->queue[0] == -1)
      delegation->delegate_idx = -1;
    return_value = 0;
  }
  
  pthread_mutex_unlock(&(delegation->delegate_mutex));
  return return_value;
}

/* Extras for Assignment 3 End */

void process_command(int nargs, char *args[], char *path, char *envp[]);

void parse_args(char *buffer, char** args, 
                size_t args_size, size_t *nargs)
{
        char *buf_args[args_size]; /* You need C99 */
        char **cp, *wbuf;
        size_t i, j;
        
        wbuf=buffer;
        buf_args[0]=buffer; 
        args[0] =buffer;
        
        for(cp=buf_args; (*cp=strsep(&wbuf, " \n\t")) != NULL ;){
                if ((*cp != NULL) && (++cp >= &buf_args[args_size]))
                        break;
        }
        
        for (j=i=0; buf_args[i]!=NULL; i++){
                if (strlen(buf_args[i]) > 0)
                        args[j++]=buf_args[i];
        }
        
        *nargs=j;
        args[j]=NULL;
}

/* this is kind of like getenv() */
char *find_env(char *envvar, char *notfound, char *envp[])
{
        const int MAXPATTERN = 128;
        int i, p;
        char c;
        char pattern[MAXPATTERN];
        char *value = NULL;

        p = 0;
        while ((c = envvar[p])) {
                pattern[p] = c;
                p++;
                if (p == (MAXPATTERN - 2)) {
                        break;
                }
        }
        pattern[p] = '=';
        p++;
        pattern[p] = '\0';
        
        i = 0;
        while (envp[i] != NULL) {
                if (strncmp(pattern, envp[i], p) == 0) {                        
                        value = envp[i] + p;
                }
                i++;
        }

        if (value == NULL) {
                return notfound;
        } else {
                return value;
        }
}

void find_binary(char *name, char *path, char *fn, int fn_size) {
        char *n, *p;
        int r, stat_return;

        struct stat file_status;

        if (name[0] == '.' || name[0] == '/') {
                strncpy(fn, name, fn_size);
                return;
        }
        
        p = path;
        while (*p != '\0') {       
                r = 0;
                while (*p != '\0' && *p != ':' && r < fn_size - 1) {
                        fn[r] = *p;
                        r++;
                        p++;
                }

                fn[r] = '/';
                r++;
                
                n = name;
                while (*n != '\0' && r < fn_size) {
                        fn[r] = *n;
                        n++;
                        r++;
                }
                fn[r] = '\0';

                
                stat_return = stat(fn, &file_status);

                if (stat_return == 0) {
                        return;
                }

                if (*p != '\0') {
                        p++;
                }
        }
}

void setup_comm_fn(char *pidstr, char *comm_fn)
{
        char *c;

        strcpy(comm_fn, proc_prefix);
        c = comm_fn + strlen(comm_fn);
        *c = '/';
        c++;
        strcpy(c, pidstr);
        c = c + strlen(pidstr);
        strcpy(c, "/comm");
}


/*
 *
 *    Assignment 2 Functions Start Here
 *
 *
 */



/* This function has been modified for Assignment-3 */

void delegate_helper() {
  int i,null_return;
  // send recursion for kids to pick it up
  for(i=0;i<BRANCH_NUM;i++) {
    if(children_pids[i]!=0) {
      null_return = write(children_feeds_fd[i],"delegate\n",9);
    }
  }
}

/* This function has been modified for Assignment-3 */

void delegate(int argc, char *argv[], char *path, char *envp[])
{
  pid_t my_pid;
  int has_children = 0;
  int i;
  int null_return; // never-ever do this in real life
  int printed_len;
  
  char *args[ARR_SIZE];      
  size_t nargs;

  
  char buffer[BUFFER_SIZE];
  buffer[0]='\0';
  
  my_pid = getpid();

  /* Check if there are children here
     
     has_children = ..... some check that you should do
     
  */
  
  if (children_pids[0]>0) has_children = 1;

  if (level_count == 0) {// I am root
  
    if (argc<2) {
#ifdef DEBUG_TREESH
      fprintf(stdout,"TreeSh %d can't delegate nothing\n", my_pid);
      fflush(stdout);
#endif
      return;
    }
    
      
    if (has_children) {
      
      // recreate the buffer
      printed_len = 0;
      for (i=1; i<argc; i++) {
	strncpy(&(buffer[printed_len]),argv[i],BUFFER_SIZE-printed_len);
	printed_len += strlen(argv[i]);
	buffer[printed_len]=' ';
	printed_len++;
      }
      buffer[printed_len]='\0';
      
      
      // post delegated command, since you're root
      post_delegated(buffer);

      // delegate to all you can
      delegate_helper();
      
      // wait for completion if root
      /* Assignment-3 has to do something here  -- hint: sychronisation between processes using a semaphore */ 
    } else {
    /*... process the command yourself */
#ifdef DEBUG_TREESH
      fprintf(stdout,"TreeSh %d can't delegate and will do the command on its own\n", my_pid);
      fflush(stdout);
#endif
      process_command(argc-1, &(argv[1]), path, envp);
    }
  } else { // I am not-root (groot?)

    if (has_children) {
      // delegate to all you cn
      delegate_helper();
    }

    // try to get the command
    if (get_delegated(my_pid, &(buffer[0])) == 0) {

#ifdef DEBUG_TREESH
      fprintf(stderr,"I %d received command:%s\n",my_pid,buffer);
#endif

      parse_args(buffer, args, ARR_SIZE, &nargs);
      process_command(nargs, args, path, envp);
    } else {
#ifdef DEBUG_TREESH
      fprintf(stderr,"I %d hoped to get a command, but got:%s\n",my_pid,buffer);
#endif
    }
  }
}

void grow(int levels)
{
  pid_t my_pid;
  pid_t child_pid;
  int child_idx,i;
  int com_pipe[2];
  int devnull_fd; 
  int null_return; // never ever do this in real life

  int grow_blocked = 0;
  
  char local_buf[128];
  
  if (levels <= 0) {
#ifdef DEBUG_TREESH
    fprintf(stdout,"TreeSh thinks you're funny, growing trees backwards\n");
    fflush(stdout);
#endif
    return;
  }
    
   my_pid = getpid();

#ifdef DEBUG_TREESH
   fprintf(stdout,"TreeSh %d is growing\n", my_pid);
   fflush(stdout);
#endif
  /*
    If you already have 2 "tree" children, then do nothing
    Otherwise fork "tree" children, until you have two.
    For every new "tree" child setup a common pipe IPC, so that the parent shell can "type in" commands for the child shell. 
    Please notice that if there was a "tree" child shell already, then the newly spawned shell child is "younger" and has to be recorded as such.
  */

   for (child_idx = 0; child_idx < BRANCH_NUM; child_idx++) {
     if (children_feeds_fd[child_idx]!=-1) {
       if ((children_feeds_fd[child_idx]!=-1) && (levels > 1)) {
	 sprintf(local_buf,"grow %d\n",levels-1);
	 null_return = write(children_feeds_fd[child_idx],local_buf,strlen(local_buf));
       }
       continue;
     }

     /* Assignment-3 Extra Begin */

     grow_blocked = sem_trywait(&delegation->grow_token);
     if (grow_blocked != 0) {

#ifdef DEBUG_TREESH
       fprintf(stdout,"BonsaiSh %d: could not get a token to grow :(\n", my_pid);
       fflush(stdout);
#endif
       
       return;
     }
     
     /* Assignment-3 Extra End */
     
     /* setup pipe */


     if (pipe(com_pipe) == -1) {
#ifdef DEBUG_TREESH
       fprintf(stdout,"TreeSh %d: could not grow any more :(\n", my_pid);
       fflush(stdout);
#endif

       /* Assignment-3 Extra Start */
       /* Assignment-3 has to do something here -- hint: pipe failed, but did we get some other resources that we need to free now?*/ 
       /* Assignment-3 Extra End */
       
       return;      
     }

     
     child_pid = fork();
     
     if (child_pid == -1) {
#ifdef DEBUG_TREESH       
       fprintf(stdout,"TreeSh %d: could not grow any more :(\n", my_pid);
       fflush(stdout);
#endif
       close(com_pipe[0]);// clean up the pipe from the system
       close(com_pipe[1]);
       

       /* Assignment-3 Extra Start */
       /* Assignment-3 has to do something here -- we just gave up the pipe, but did we get some other resources that we need to free now?*/ 
       /* Assignment-3 Extra End */

       return;
     }

     if(child_pid > 0) { // this is the parent process
       close(com_pipe[0]); // close input end of the pipe
       children_feeds_fd[child_idx] = dup(com_pipe[1]);
       close(com_pipe[1]);
       children_pids[child_idx] = child_pid;
     } else {
#ifdef DEBUG_TREESH
       fprintf(stdout,"I'm a child with PID %d\n",getpid());
       fflush(stdout);
#endif
       // this is child process
       // need to begin by clearing the "children" record.
       for(i=0;i<BRANCH_NUM;i++) {
	 if(children_feeds_fd[i]!=-1) {
	   close(children_feeds_fd[i]);
	 }
	 children_feeds_fd[i]=-1;
	 children_pids[i] = -1;
       }
       // resetting the command count
       command_count = 0;
       //altering the STDIN
       dup2(com_pipe[0], 0);
       close(com_pipe[0]);
       close(com_pipe[1]);
       
       level_count = level_count + 1; // acknowledge that you're not the root of the grow tree

       /* Assignment-3 Extra Start */
       insert_delegate(getpid());
       /* Assignment-3 Extra End */
       
       //finally grow some more :) if needed
       if(levels>1)
	 grow(levels-1);
       return; // and then go back to the prompt loop
     }
   }
   
#ifdef DEBUG_TREESH
   fprintf(stdout,"TreeSh %d: my tree is now fully grown (afaik)\n", my_pid);
   fflush(stdout);
#endif
}

void prune()
{
  pid_t my_pid;
  int i,null_return;
  
  my_pid = getpid();

#ifdef DEBUG_TREESH
  fprintf(stdout,"TreeSh %d is pruning\n", my_pid);
  fflush(stdout);
#endif
  
  /*
    If there are "tree" children, abort the eldest (and all its descendants), so that only one "tree" child remains.
    Make sure that the youngest/remaining "tree" child also "prunes" its subtree.
   */

  for(i=0;i<BRANCH_NUM-1;i++){
    if(children_pids[i]>0) {
      if (children_pids[i+1]==-1) {
#ifdef DEBUG_TREESH
	fprintf(stdout,"TreeSh %d is pruning, but there are too few kids\n", my_pid);
	fflush(stdout);
#endif
	break;// you've found the last one.
      }
      null_return = write(children_feeds_fd[i],"exit\n",5);
    }
    children_pids[i] = -1;
    close(children_feeds_fd[i]);
    children_feeds_fd[i]=-1;
  }

  if (i>0) {
    children_pids[0] = children_pids[i];
    children_feeds_fd[0] = children_feeds_fd[i];
    if (BRANCH_NUM > 1) {
      children_pids[i]=-1;
      children_feeds_fd[i]=-1;
    }
  }
  // send the remaining child the prune command too
  null_return = write(children_feeds_fd[0],"prune\n",6);
  
#ifdef DEBUG_TREESH
  fprintf(stdout,"TreeSh %d is now a single branch\n", my_pid);
  fflush(stdout);
#endif  
}

void uproot()
{
  pid_t my_pid;
  int null_return,i;
  
  my_pid = getpid();

#ifdef DEBUG_TREESH
  fprintf(stdout,"TreeSh %d is uprooting\n", my_pid);
  fflush(stdout);
#endif
  /*
    Abort all "tree" children and their sub-trees.
   */

  for(i=0;i<BRANCH_NUM;i++){
    if(children_pids[i]>0) {
      null_return = write(children_feeds_fd[i],"exit\n",5);
    }
    children_pids[i] = -1;
    close(children_feeds_fd[i]);
    children_feeds_fd[i]=-1;
  }

#ifdef DEBUG_TREESH
  fprintf(stdout,"TreeSh %d is all alone\n", my_pid);
  fflush(stdout);
#endif
}

/*
 *
 *    Assignment 2 Functions End Here --- but there are other places where things were changed
 *
 *
 */


void plist()
{
        DIR *proc;
        struct dirent *e;
        int result;
        char comm[COMM_SIZE];  /* seems to just need 16 */        
        char comm_fn[512];
        int fd, i, n;

        proc = opendir(proc_prefix);

        if (proc == NULL) {
                fprintf(stderr, "ERROR: Couldn't open /proc.\n");
        }
        
        for (e = readdir(proc); e != NULL; e = readdir(proc)) {
                if (isdigit(e->d_name[0])) {
                        setup_comm_fn(e->d_name, comm_fn);
                        fd = open(comm_fn, O_RDONLY);
                        if (fd > -1) {                                
                                n = read(fd, comm, COMM_SIZE);
                                close(fd);
                                for (i=0; i < n; i++) {
                                        if (comm[i] == '\n') {
                                                comm[i] = '\0';
                                                break;
                                        }
                                }
				if (level_count == 0) 
				  fprintf(stdout,"%s: %s\n", e->d_name, comm);
                        } else {
			  if (level_count == 0)
			    fprintf(stdout,"%s\n", e->d_name);
                        }
                }
        }
	fflush(stdout);
        
        result = closedir(proc);
        if (result) {
                fprintf(stderr, "ERROR: Couldn't close /proc.\n");
        }
}

void fix_coms_table(int pid) {
  int i,j;


  // one of the communicable kids died -- fix the com tables
  
  for(i=0;i<BRANCH_NUM; i++) {
    if(children_pids[i]==pid) {
      children_pids[i] = -1;
      close(children_feeds_fd[i]);
      children_feeds_fd[i]=-1;
      break;
    }
  }
  for(j=i;j<BRANCH_NUM-1;j++) {
    children_pids[j]=children_pids[j+1];
    children_pids[j+1] = -1;
    children_feeds_fd[j] = children_feeds_fd[j+1];
    children_feeds_fd[j+1] = -1;
  }
  
}

void signal_handler(int the_signal)
{
        int pid, status;
	
        if (the_signal == SIGHUP) {
                fprintf(stderr, "Received SIGHUP.\n");
                return;
        }
        
        if (the_signal != SIGCHLD) {
                fprintf(stderr, "Child handler called for signal %d?!\n",
                        the_signal);
                return;
        }

        pid = waitpid(-1,&status,WNOHANG);

        if (pid == -1) {
                /* nothing to wait for */
                return;
        }
        
#ifdef DEBUG_TREESH
        if (WIFEXITED(status)) {
                fprintf(stderr, "\nProcess %d exited with status %d.\n",
                        pid, WEXITSTATUS(status));
        } else {
                fprintf(stderr, "\nProcess %d aborted.\n", pid);
        }
#endif
	fix_coms_table(pid);
}

void run_program(char *args[], int background, char *stdout_fn,
                 char *path, char *envp[])
{
        pid_t pid;
        int fd, *ret_status = NULL;
        char bin_fn[BUFFER_SIZE];

        pid = fork();
        if (pid) {
                if (background) {
                        fprintf(stderr,
                                "Process %d running in the background.\n",
                                pid);
                } else {
                        pid = wait(ret_status);
                }
        } else {
                find_binary(args[0], path, bin_fn, BUFFER_SIZE);

                if (stdout_fn != NULL) {
                        fd = creat(stdout_fn, 0666);
                        dup2(fd, 1);
                        close(fd);
                }
                
                if (execve(bin_fn, args, envp)!=0) {
                        puts(strerror(errno));
                        exit(127);
                }
        }
}

void exit_prep() {

  int i, null_return;
  
  for(i = BRANCH_NUM-1; i>=0; i--) {
    if(children_pids[i]!=-1){
	null_return = write(children_feeds_fd[i],"exit\n",5);
    }
  }

  delete_delegate(getpid());

  if (level_count > 0) {
    /* Assignment-3 has to do something here -- hint: if this process was a helper in the tree, then it used some particular token. It no longer needs it. */
  }

  sleep(1);
}

void process_command(int nargs, char *args[], char *path, char *envp[])
{
  int background;
  int grow_depth;
  char *stdout_fn;
  int i, j;
        
  
  if (!strcmp(args[0], "exit")) {
#ifdef DEBUG_TREESH
    fprintf(stdout,"Process %d exits\n",getpid());
    fflush(stdout);
#endif

    exit_prep();
    
    exit(0);
  }
       
  if (!strcmp(args[0], "plist")) {
    plist();
    return;
  }

  if (!strcmp(args[0], "delegate")) {
    delegate(nargs,args,path,envp);
    return;
  }
       
  if (!strcmp(args[0], "grow")) {
    if(nargs!=2) {
      fprintf(stderr, "ERROR: Bad list of arguments: grow <number>\n");
      return;
    }
    if(sscanf(args[1],"%d",&grow_depth)==1){
      grow(grow_depth);
    } else {
      fprintf(stderr, "ERROR: Bad list of arguments: grow <number>\n");
    }
    return;
  }
       
  if (!strcmp(args[0], "prune")) {
    prune();
    return;
  }
       
  if (!strcmp(args[0], "uproot")) {
    uproot();
    return;
  }

  /* Assignment-3 Extra Begins */
  if (!strcmp(args[0], "noop")) {
    // nothing to do
#ifdef DEBUG_TREESH
    fprintf(stdout,"Nothing burger requested and served by %d\n",getpid());
    fflush(stdout);
#endif
    return;
  }
  /* Assignment-3 Extra Ends */
  
  /* printf("Not internal command, eh? Need a nap.. $ "); */
  /* fflush(stdout); */
  /* sleep(10); */
  /* printf("... ok... now I'm ready to run your request... $"); */
  /* fflush(stdout); */
		
		
  background = 0;            
  if (strcmp(args[nargs-1], "&") == 0) {
    background	= 1;
    nargs--;
    args[nargs]	= NULL;
  }

  stdout_fn = NULL;
  for (i = 1; i < nargs; i++) {
    if (args[i][0] == '>') {
      stdout_fn	= args[i];
      stdout_fn++;
#ifdef DEBUG_TREESH
      fprintf(stdout,"Set stdout to %s\n", stdout_fn);
      fflush(stdout);
#endif
      for (j = i; j < nargs - 1; j++) {
	args[j] = args[j+1];
      }
      nargs--;
      args[nargs] = NULL;
      break;
    }
  }
                
  run_program(args, background, stdout_fn, path, envp);
  //	command_count = command_count + 1;  
}

void prompt_loop(char *username, char *path, char *envp[])
{
        char buffer[BUFFER_SIZE];
        char *args[ARR_SIZE];
        
        size_t nargs;
        char *s;

        while(1){
	  if ( level_count == 0) {
	        fprintf(stdout,"%s [%d]$ ", username, command_count++);
		fflush(stdout);
	  }
                s = fgets(buffer, BUFFER_SIZE, stdin);
                
                if (s == NULL) {
                        /* we reached EOF */
		  if (level_count == 0) {
		        fprintf(stdout,"\n");
			fflush(stdout);
		  }
                        exit(0);
                }

#ifdef DEBUG_TREESH
		if (level_count == 0) {
		  fprintf(stdout,"%s",buffer);
		  fflush(stdout);
		}
#endif
                parse_args(buffer, args, ARR_SIZE, &nargs); 

                if (nargs==0) continue;

		process_command(nargs, args, path, envp);	
	}    
}

void init_shared(shared *s)
{
  int i;
  
  /* We need to explicitly mark the mutex as shared or risk undefined behavior */
  pthread_mutexattr_t mattr = {};
  pthread_mutexattr_setpshared(&mattr, 1);
  pthread_mutex_init(&s->delegate_mutex, &mattr);
  
  sem_init(&(s->grow_token), 1, MAX_BONSAI_SIZE);
  sem_init(&(s->delegator_token), 1, 0);

  s->delegate_idx = -1;
  s->buffer[0] = '\0';
  
  for (i=0; i<MAX_BONSAI_SIZE; i++) {
    s->queue[i] = -1;
  }
}

int main(int argc, char *argv[], char *envp[])
{
        struct sigaction signal_handler_struct;
	
        char *username;
        char *default_username = "UNKNOWN";
        
        char *path;
        char *default_path = "/usr/bin:/bin";

	char *tree_max_str = NULL;
	
	int i, tree_max_val;
	
        memset (&signal_handler_struct, 0, sizeof(signal_handler_struct));
        signal_handler_struct.sa_handler = signal_handler;
        signal_handler_struct.sa_flags = SA_RESTART;
        
        if (sigaction(SIGCHLD, &signal_handler_struct, NULL)) {
                fprintf(stderr, "Couldn't register SIGCHLD handler.\n");
        }
        
        if (sigaction(SIGHUP, &signal_handler_struct, NULL)) {
                fprintf(stderr, "Couldn't register SIGHUP handler.\n");
        }
        
        username = find_env("USER", default_username, envp);
        path = find_env("PATH", default_path, envp);

	for (i=0;i<BRANCH_NUM;i++) {
	  children_feeds_fd[i]=-1;
	  children_pids[i] = -1;
	}

        delegation = (shared *) mmap(NULL, sizeof(shared),
			    PROT_READ|PROT_WRITE,
			    MAP_SHARED|MAP_ANONYMOUS, -1, 0);

        if (delegation == MAP_FAILED)
        {
	  fprintf(stderr, "Error: Unable to mmap: %s\n", strerror(errno));
	  exit(-1);
        }

	init_shared(delegation);
	
        prompt_loop(username, path, envp);
        
        return 0;
}
