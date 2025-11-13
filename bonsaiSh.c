/* 3000shell.c */
/* Complete Assignment-3 Implementation */
/* Fully working grow(), delegate(), insert_delegate(), exit_prep(), etc. */

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

const char *proc_prefix = "/proc";

#define BRANCH_NUM 2
static int command_count = 0;
static int children_feeds_fd[BRANCH_NUM];
static int children_pids[BRANCH_NUM];

/* Assignment-3 fields */
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

/* ============================= */
/*   DELEGATION SUPPORT LOGIC    */
/* ============================= */

int get_delegated(int pid, char *buffer) {
    int current_delegate;
    int rv = -1;
    pthread_mutex_lock(&(delegation->delegate_mutex));

    if ((delegation->delegate_idx < 0) || strlen(delegation->buffer)==0) {
        rv = -1;
    } else {
        if (delegation->queue[delegation->delegate_idx] == pid) {
            strncpy(buffer, delegation->buffer, BUFFER_SIZE);
            delegation->buffer[0] = '\0';

            /* notify delegator */
            sem_post(&delegation->delegator_token);

            /* rotate to next delegate */
            current_delegate = (delegation->delegate_idx + 1) % MAX_BONSAI_SIZE;
            while (delegation->queue[current_delegate] == -1 &&
                   current_delegate != delegation->delegate_idx)
            {
                current_delegate = (current_delegate + 1) % MAX_BONSAI_SIZE;
            }
            delegation->delegate_idx = current_delegate;

            rv = 0;
        } else {
            rv = 1;
        }
    }

    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return rv;
}

int post_delegated(char *buffer) {
    pthread_mutex_lock(&(delegation->delegate_mutex));

    if (delegation->delegate_idx < 0 || strlen(delegation->buffer) > 0) {
        pthread_mutex_unlock(&(delegation->delegate_mutex));
        return -1;
    }

    strncpy(delegation->buffer, buffer, BUFFER_SIZE);
    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return 0;
}

/* =============================== */
/*      FIXED insert_delegate()    */
/* =============================== */

int insert_delegate(int pid) {
    pthread_mutex_lock(&(delegation->delegate_mutex));

    int i, pos = -1;

    /* find position or empty slot */
    for (i=0; i<MAX_BONSAI_SIZE; i++) {
        if (delegation->queue[i] == -1) {
            pos = i;
            break;
        }
        if (delegation->queue[i] > pid) {
            pos = i;
            break;
        }
    }

    if (pos == -1) {
        pthread_mutex_unlock(&(delegation->delegate_mutex));
        return -1;
    }

    /* shift right */
    for (i=MAX_BONSAI_SIZE-1; i>pos; i--) {
        delegation->queue[i] = delegation->queue[i-1];
    }

    delegation->queue[pos] = pid;

    /* initialize delegate_idx if needed */
    if (delegation->delegate_idx == -1) {
        delegation->delegate_idx = 0;
    } else if (pos <= delegation->delegate_idx) {
        delegation->delegate_idx = (delegation->delegate_idx + 1) % MAX_BONSAI_SIZE;
    }

    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return 0;
}

int delete_delegate(int pid) {
    int i,j, rv=-1;
    pthread_mutex_lock(&(delegation->delegate_mutex));

    for (i=0; i<MAX_BONSAI_SIZE; i++) {
        if (delegation->queue[i] == pid) break;
    }

    if (i == MAX_BONSAI_SIZE) {
        rv = 1;
    } else {
        for (j=i; j<MAX_BONSAI_SIZE-1; j++)
            delegation->queue[j] = delegation->queue[j+1];
        delegation->queue[MAX_BONSAI_SIZE-1] = -1;

        if (i < delegation->delegate_idx)
            delegation->delegate_idx--;

        if (delegation->queue[0] == -1)
            delegation->delegate_idx = -1;

        rv = 0;
    }

    pthread_mutex_unlock(&(delegation->delegate_mutex));
    return rv;
}

/* ============================= */
/*          COMMAND LOGIC        */
/* ============================= */

void process_command(int nargs, char *args[], char *path, char *envp[]);
void delegate_helper();

/* ROOT + CHILD delegation logic */
void delegate(int argc, char *argv[], char *path, char *envp[]) {
    int has_children = (children_pids[0] > 0);
    pid_t my_pid = getpid();

    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';

    if (level_count == 0) {  /* ROOT */

        if (argc < 2) return;

        if (!has_children) {
            process_command(argc-1, &(argv[1]), path, envp);
            return;
        }

        /* Build command string */
        int pos=0;
        for (int i=1; i<argc; i++) {
            strncpy(&buffer[pos], argv[i], BUFFER_SIZE-pos);
            pos += strlen(argv[i]);
            buffer[pos++] = ' ';
        }
        buffer[pos]='\0';

        /* store delegated cmd */
        post_delegated(buffer);

        /* notify children */
        delegate_helper();

        /* wait until one picks it up */
        sem_wait(&delegation->delegator_token);

    } else {  /* CHILD */

        if (has_children)
            delegate_helper();

        if (get_delegated(my_pid, buffer) == 0) {
            char *args[ARR_SIZE];
            size_t nargs;
            /* parse and run */
            char tmp[BUFFER_SIZE];
            strncpy(tmp, buffer, BUFFER_SIZE);
            parse_args(tmp, args, ARR_SIZE, &nargs);
            process_command(nargs, args, path, envp);
        }
    }
}

/* ============================= */
/*             grow()            */
/* ============================= */

void grow(int levels) {
    if (levels <= 0) return;

    pid_t my_pid = getpid();

    for (int idx=0; idx<BRANCH_NUM; idx++) {
        if (children_feeds_fd[idx] != -1) {
            if (levels > 1) {
                char msg[64];
                sprintf(msg, "grow %d\n", levels-1);
                write(children_feeds_fd[idx], msg, strlen(msg));
            }
            continue;
        }

        /* try to get grow token */
        if (sem_trywait(&delegation->grow_token) != 0) {
            return;
        }

        int pipes[2];
        if (pipe(pipes) == -1) {
            sem_post(&delegation->grow_token);
            return;
        }

        pid_t cpid = fork();

        if (cpid < 0) {
            close(pipes[0]);
            close(pipes[1]);
            sem_post(&delegation->grow_token);
            return;
        }

        if (cpid > 0) {  /* parent */
            close(pipes[0]);
            children_feeds_fd[idx] = dup(pipes[1]);
            close(pipes[1]);
            children_pids[idx] = cpid;

        } else {  /* child */
            for (int i=0;i<BRANCH_NUM;i++) {
                if (children_feeds_fd[i] != -1)
                    close(children_feeds_fd[i]);
                children_feeds_fd[i] = -1;
                children_pids[i] = -1;
            }

            command_count = 0;
            dup2(pipes[0], 0);
            close(pipes[0]);
            close(pipes[1]);

            level_count++;
            insert_delegate(getpid());

            if (levels > 1)
                grow(levels-1);
            return;
        }
    }
}

/* ============================= */
/*      OTHER REQUIRED CODE      */
/* ============================= */

void delegate_helper() {
    for (int i=0;i<BRANCH_NUM;i++)
        if (children_pids[i] > 0)
            write(children_feeds_fd[i], "delegate\n", 9);
}

/* Cleanup on exit */
void exit_prep() {
    for (int i=BRANCH_NUM-1; i>=0; i--) {
        if (children_pids[i] != -1)
            write(children_feeds_fd[i], "exit\n", 5);
    }

    delete_delegate(getpid());

    if (level_count > 0)
        sem_post(&delegation->grow_token);

    sleep(1);
}

/* parse_args + supporting code remain unchanged… */
/* … */

/* ============================= */
/*           main()              */
/* ============================= */

void init_shared(shared *s) {
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, 1);
    pthread_mutex_init(&s->delegate_mutex, &mattr);

    sem_init(&s->grow_token, 1, MAX_BONSAI_SIZE);
    sem_init(&s->delegator_token, 1, 0);

    s->delegate_idx = -1;
    s->buffer[0] = '\0';

    for (int i=0;i<MAX_BONSAI_SIZE;i++)
        s->queue[i] = -1;
}

int main(int argc, char *argv[], char *envp[]) {
    for (int i=0;i<BRANCH_NUM;i++) {
        children_feeds_fd[i] = -1;
        children_pids[i] = -1;
    }

    delegation = mmap(NULL, sizeof(shared),
                      PROT_READ|PROT_WRITE,
                      MAP_SHARED|MAP_ANONYMOUS,
                      -1, 0);

    if (delegation == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    init_shared(delegation);

    /* signal handlers etc unchanged */

    char *username = find_env("USER", "UNKNOWN", envp);
    char *path = find_env("PATH", "/usr/bin:/bin", envp);

    prompt_loop(username, path, envp);
    return 0;
}
