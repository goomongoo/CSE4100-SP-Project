/* $begin shellmain */
#include "myshell.h"
#include "list.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
// parsing and evaluation
void    eval(char *cmdline);
void    run_pipe(char **argv, int fd);
char    **get_next_argv(char **argv);
int     parseline(char *buf, char **argv);
int     check_group(char **argv);

// built-in commands
int     builtin_command(char **argv); 
void    builtin_cd(char *path);
void    builtin_fg(char *arg);
void    builtin_bg(char *arg);
void    builtin_kill(char *arg);

// functions related to jobs
struct list_item    *add_job(pid_t, enum e_condition cond, char *cmd);
void                remove_job(struct list_item *job);
struct list_item    *get_last_job();
struct list_item    *get_job_by_jobid(int id);
struct list_item    *get_job_by_pid(pid_t pid);
void                set_last_job(struct list_item *job);
void                print_one_job(struct list_elem *elem);
void                print_jobs();
int                 check_jobs();
void                clear_jobs();
void                toggle_ampersand(char *cmdline, enum e_condition condition);

// signal handlers
void    sigint_handler(int sig);
void    sigchld_handler(int sig);
void    sigtstp_handler(int sig);
/* Function prototypes end */

// volatile variables
volatile int            fg_flag;
volatile int            stop_flag;
volatile int            pipe_flag;

// jobs
struct list job_list;
struct list job_stack;

const char  *CONDITION_MSG[6] = { NULL, "Running\t", "Terminated", "Stopped\t", "Done\t" };


int main(void) {
    char    cmdline[MAXLINE]; /* Command line */

    Signal(SIGINT, sigint_handler);
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGTSTP, sigtstp_handler);

    list_init(&job_list);
    list_init(&job_stack);

    while (1) {
	    /* Read */
	    printf("CSE4100-SP-P2> ");
	    fgets(cmdline, MAXLINE, stdin); 
	    if (feof(stdin))
	        exit(0);

	    /* Evaluate */
	    eval(cmdline);
    }
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
    char                *argv[MAXARGS]; /* Argument list execve() */
    char                buf[MAXLINE];   /* Holds modified command line */
    int                 bg;             /* Should the job run in bg or fg? */
    pid_t               pid;            /* Process id */
    int                 status;
    char                *cmd_copy;
    struct list_item    *job;

    sigset_t    mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    
    cmd_copy = Malloc(sizeof(char) * (strlen(buf) + 3));
    strcpy(buf, cmdline);
    strcpy(cmd_copy, buf);
    bg = parseline(buf, argv);

    while(check_jobs());

    /* Ignore empty lines */
    if (argv[0] == NULL)  
	    return;

    if (builtin_command(argv))
        return;
    
    if (!bg) { // Foreground job
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if ((pid = Fork()) == 0) { // child process
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            if (check_group(argv))
                setpgrp();
            if (!*get_next_argv(argv)) { // single command
                if (execvp(argv[0], argv) < 0) {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            } else { // piped command
                pipe_flag = 1;
                run_pipe(argv, 0);
            }
        } else { // parent process
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            fg_flag = 1;
            job = add_job(pid, FG, cmd_copy);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            if (waitpid(pid, &status, WUNTRACED) < 0)
                unix_error("waitfg: waitpid error");
            else {
                pipe_flag = 0;
                fg_flag = 0;
                if (WIFSTOPPED(status)) {
                    job->condition = STOP;
                    print_one_job(&job->elem);
                } else if (WIFEXITED(status))
                    remove_job(job);
            }
        }
    } else { // background job
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if ((pid = Fork()) == 0) { // child process
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            if (check_group(argv))
                setpgrp();
            if (!*get_next_argv(argv)) { // single command
                if (execvp(argv[0], argv) < 0) {
                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            } else { // piped command
                pipe_flag = 1;
                run_pipe(argv, 0);
            }
        } else { // parent process
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            fg_flag = 0;
            job = add_job(pid, BG, cmd_copy);
            printf("[%d] %d\n", job->job_id, job->pid);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
        }
    }

    return;
}

/* Run piped commands recursively */
void run_pipe(char **argv, int fd) {
    int     pipefd[2];
    int     status;
    pid_t   pid;
    char    **argv_next;

    sigset_t    mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    argv_next = get_next_argv(argv);
    if (!*argv_next) {
        dup2(fd, 0);
        close(fd);
        if (builtin_command(argv))
            exit(0);
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(0);
        }
    } else {
        if (pipe(pipefd) < 0)
            unix_error("Pipe error");
        if ((pid = Fork()) == 0) { // child
            close(pipefd[0]);
            dup2(fd, 0);
            close(fd);
            dup2(pipefd[1], 1);
            close(pipefd[1]);
            if (builtin_command(argv))
                exit(0);
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        } else { // parent
            close(pipefd[1]);
            run_pipe(argv_next, pipefd[0]);
            if (waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpid error");
        }
    }
}

/* Get next argv (command), for checking pipe */
char **get_next_argv(char **argv) {
    while (*argv)
        argv++;
    
    return argv + 1;
}

/* Check for grouping child process(es) */
int check_group(char **argv) {
    char    **argv_base;

    argv_base = argv;
    if (!*get_next_argv(argv)) { // no pipe
        if (!strcmp(argv[0], "cat") && !argv[1])
            return 0;
        else if (!strcmp(argv[0], "grep") && argv[1])
            return 0;
        else if (!strcmp(argv[0], "less") && argv[1])
            return 0;
        else
            return 1;
    } else { // pipe
        while (argv[0]) {
            if (!strcmp(argv[0], "cat") && !argv[1])
                return 0;
            else if (!strcmp(argv[0], "grep")) {
                if ((argv == argv_base && argv[1]) || (argv != argv_base && !argv[1]))
                    return 0;
            } else if (!strcmp(argv[0], "less")) {
                if ((argv == argv_base && argv[1]) || (argv != argv_base && !argv[1]))
                    return 0;
            }
            argv = get_next_argv(argv);
        }
        return 1;
    }
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "exit")) {
        clear_jobs();
	    exit(0);
    } /* quit command */

    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    
    if (!strcmp(argv[0], "cd")) {
        builtin_cd(argv[1]);
        return 1;
    }

    if (!strcmp(argv[0], "jobs")) {
        print_jobs();
        return 1;
    }

    if (!strcmp(argv[0], "fg")) {
        builtin_fg(argv[1]);
        return 1;
    }

    if (!strcmp(argv[0], "bg")) {
        builtin_bg(argv[1]);
        return 1;
    }

    if (!strcmp(argv[0], "kill")) {
        if (!argv[1] || argv[1][0] != '%')
            return 0;
        builtin_kill(argv[1]);
        return 1;
    }
    
    return 0;                     /* Not a builtin command */
}

/* run cd command */
void builtin_cd(char *path) {  
    if (!path || !strcmp(path, "~")) {
        chdir(getenv("HOME"));
        return;
    }

    if (chdir(path) == -1)
        printf("myshell: cd: %s: No such file or directory\n", path);
}

void builtin_fg(char *arg) {
    struct list_item    *job_target;
    pid_t               pid;
    int                 arg_job_id;
    int                 status;

    sigset_t    mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    if (!arg) {
        if (list_empty(&job_list)) {
            printf("myshell: fg: current: no such job\n");
            return;
        }
        job_target = get_last_job();
        pid = job_target->pid;
        if (job_target->condition == STOP || job_target->condition == BG) {
            toggle_ampersand(job_target->cmdline, FG);
            printf("%s\n", job_target->cmdline);
            job_target->condition = FG;
            fg_flag = 1;
            stop_flag = 0;
            Kill(-pid, SIGCONT);
        }
    } else {
        if (*arg == '%')
            arg_job_id = atoi(arg + 1);
        else
            arg_job_id = atoi(arg);
        job_target = get_job_by_jobid(arg_job_id);
        if (!job_target) {
            printf("myshell: fg: %s: no such job\n", arg);
            return;
        }
        pid = job_target->pid;
        if (job_target->condition == STOP || job_target->condition == BG) {
            toggle_ampersand(job_target->cmdline, FG);
            printf("%s\n", job_target->cmdline);
            job_target->condition = FG;
            set_last_job(job_target);
            fg_flag = 1;
            stop_flag = 0;
            Kill(-pid, SIGCONT);
        }
    }
    if (waitpid(pid, &status, WUNTRACED) < 0)
        unix_error("waitfg: waitpid error");
    else {
        pipe_flag = 0;
        fg_flag = 0;
        if (WIFSTOPPED(status)) {
            job_target->condition = STOP;
            print_one_job(&job_target->elem);
        } else if (WIFEXITED(status))
            remove_job(job_target);
    }
}

void print_bg_job(struct list_item *item) {
    struct list_elem    *elem;
    struct list_elem    *stack_top;

    stack_top = list_front(&job_stack);
    elem = &item->elem;
    if (elem == list_entry(stack_top, struct list_item, elem)->elem_ptr)
            printf("[%d]+  %s\n", item->job_id, item->cmdline);
        else if (elem == list_entry(list_next(stack_top), struct list_item, elem)->elem_ptr)
            printf("[%d]-  %s\n", item->job_id, item->cmdline);
        else
            printf("[%d]   %s\n", item->job_id, item->cmdline);
}

void builtin_bg(char *arg) {
    struct list_item    *job_target;
    pid_t               pid;
    int                 arg_job_id;

    sigset_t    mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    if (!arg) {
        if (list_empty(&job_list)) {
            printf("myshell: bg: current: no such job\n");
            return;
        }
        job_target = get_last_job();
        pid = job_target->pid;
        if (job_target->condition == BG) {
            printf("myshell: bg: job %d already in background\n", job_target->job_id);
            return;
        } else if (job_target->condition == STOP) {
            toggle_ampersand(job_target->cmdline, BG);
            job_target->condition = BG;
            print_bg_job(job_target);
            stop_flag = 0;
            Kill(-pid, SIGCONT);
        }
    } else {
        if (*arg == '%')
            arg_job_id = atoi(arg + 1);
        else
            arg_job_id = atoi(arg);
        job_target = get_job_by_jobid(arg_job_id);
        if (!job_target) {
            printf("myshell: bg: %s: no such job\n", arg);
            return;
        }
        pid = job_target->pid;
        if (job_target->condition == BG) {
            printf("myshell: bg: job %d already in background\n", job_target->job_id);
            return;
        } else if (job_target->condition == STOP) {
            toggle_ampersand(job_target->cmdline, BG);
            job_target->condition = BG;
            print_bg_job(job_target);
            stop_flag = 0;
            Kill(-pid, SIGCONT);
        }
    }
}

void builtin_kill(char *arg) {
    struct list_item    *job_target;
    pid_t               pid;
    int                 arg_job_id;

    arg_job_id = atoi(arg + 1);
    job_target = get_job_by_jobid(arg_job_id);
    if (!job_target) {
        printf("myshell: kill: %s: no such job\n", arg);
        return;
    }
    pid = job_target->pid;
    job_target->condition = DIE;
    Kill(-pid, SIGINT);
    print_one_job(&job_target->elem);
    remove_job(job_target);
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) {
    char    *delim;         /* Points to first space delimiter */
    int     argc;            /* Number of args */
    int     bg;              /* Background job? */
    char    *temp;

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	    buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        if (*buf == '\"') { /* " as delimiter, ex.) "   asdfd dfsd " -> one arg */
            buf++;
            delim = strchr(buf, '\"');
        } else if (*buf == '\'') { /* ' as delimiter, ex.) 'sdfd sdf  df' -> one arg */
            buf++;
            delim = strchr(buf, '\'');
        }
	    argv[argc++] = buf;
        if ((temp = strchr(buf, '|')) && delim > temp) {
            *temp = '\0';
            argv[argc++] = NULL;
            buf = temp + 1;
            while (*buf && (*buf == ' '))
                buf++;
            delim = strchr(buf, ' ');
            argv[argc++] = buf;
        }
	    *delim = '\0';
	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
        if (*buf == '|') {
            argv[argc++] = NULL;
            buf++;
            while (*buf && (*buf == ' '))
                buf++;
        }
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	    return 1;

    /* Should the job run in the background? */
    temp = argv[argc - 1];
    if (*argv[argc-1] == '&') {
	    argv[--argc] = NULL;
        bg = 1;
    } else if (temp[strlen(temp) - 1] == '&') {
        temp[strlen(temp) - 1] = '\0';
        bg = 1;
    } else
        bg = 0;
    
    argv[++argc] = NULL;

    return bg;
}
/* $end parseline */

/* $begin functions related to job */
struct list_item *add_job(pid_t pid, enum e_condition cond, char *cmd) {
    struct list_item    *job;
    struct list_item    *stack_item;
    char                *temp;

    job = (struct list_item *)Malloc(sizeof(struct list_item));
    stack_item = (struct list_item *)Malloc(sizeof(struct list_item));

    temp = &cmd[strlen(cmd) - 2];
    while (*temp == ' ')
        temp--;
    if (*temp == '&' && *(temp - 1) != ' ') {
        *temp++ = ' ';
        *temp = '&';
    }
    *(temp + 1) = '\0';
    
    list_push_back(&job_list, &job->elem);
    if (cond == FG)
        list_push_front(&job_stack, &stack_item->elem);
    else
        list_push_back(&job_stack, &stack_item->elem);
    stack_item->elem_ptr = &job->elem;
    job->elem_ptr = &stack_item->elem;
    job->pid = pid;
    job->condition = cond;
    job->cmdline = cmd;
    if (list_empty(&job_list))
        job->job_id = 1;
    else
        job->job_id = list_entry(list_prev(&job->elem), struct list_item, elem)->job_id + 1;
    
    return job;
}

void remove_job(struct list_item *job) {
    struct list_item    *stack_item;

    stack_item = list_entry(job->elem_ptr, struct list_item, elem);
    list_remove(&stack_item->elem);
    free(stack_item);
    free(job->cmdline);
    list_remove(&job->elem);
    free(job);
}

struct list_item *get_last_job() {
    struct list_item    *stack_item;
    struct list_item    *job;

    if (list_empty(&job_stack))
        return NULL;
    stack_item = list_entry(list_front(&job_stack), struct list_item, elem);
    job = list_entry(stack_item->elem_ptr, struct list_item, elem);
    return job;
}

struct list_item *get_job_by_jobid(int id) {
    struct list_elem    *iter;
    struct list_item    *job;

    for (iter = list_begin(&job_list); iter != list_end(&job_list); iter = list_next(iter)) {
        job = list_entry(iter, struct list_item, elem);
        if (id == job->job_id)
            return job;
    }
    return NULL;
}

struct list_item *get_job_by_pid(pid_t pid) {
    struct list_elem    *iter;
    struct list_item    *job;

    for (iter = list_begin(&job_list); iter != list_end(&job_list); iter = list_next(iter)) {
        job = list_entry(iter, struct list_item, elem);
        if (pid == job->pid)
            return job;
    }
    return NULL;
}

void set_last_job(struct list_item *job) {
    struct list_elem    *stack_elem;

    stack_elem = job->elem_ptr;
    list_remove(stack_elem);
    list_push_front(&job_stack, stack_elem);
}

void print_one_job(struct list_elem *elem) {
    struct list_elem    *stack_top;
    struct list_item    *item;

    stack_top = list_front(&job_stack);
    item = list_entry(elem, struct list_item, elem);
    if (elem == list_entry(stack_top, struct list_item, elem)->elem_ptr)
            printf("[%d]+  %s                       %s\n", item->job_id, CONDITION_MSG[item->condition], item->cmdline);
        else if (elem == list_entry(list_next(stack_top), struct list_item, elem)->elem_ptr)
            printf("[%d]-  %s                       %s\n", item->job_id, CONDITION_MSG[item->condition], item->cmdline);
        else
            printf("[%d]   %s                       %s\n", item->job_id, CONDITION_MSG[item->condition], item->cmdline);
}

void print_jobs() {
    struct list_elem    *iter;
    struct list_item    *job;

    if (list_empty(&job_list))
        return;
    for (iter = list_begin(&job_list); iter != list_end(&job_list); iter = list_next(iter))
        print_one_job(iter);
}

int check_jobs() {
    struct list_elem    *iter;
    struct list_item    *item;

    if (list_empty(&job_list))
        return 0;
    for (iter = list_begin(&job_list); iter != list_end(&job_list); iter = list_next(iter)) {
        item = list_entry(iter, struct list_item, elem);
        if (item->condition == DONE) {
            toggle_ampersand(item->cmdline, DONE);
            print_one_job(iter);
            remove_job(item);
            return 1;
        }
    }
    return 0;
}

void clear_jobs() {
    struct list_item    *item;

    while (!list_empty(&job_list)) {
        item = list_entry(list_pop_front(&job_list), struct list_item, elem);
        free(item->cmdline);
        free(item);
    }
    while (!list_empty(&job_stack)) {
        item = list_entry(list_pop_front(&job_stack), struct list_item, elem);
        free(item);
    }
}

/* cmdline toggle ampersand(&) */
void toggle_ampersand(char *cmdline, enum e_condition condition) {
    int     len;
    char    *temp;

    len = strlen(cmdline);
    if (condition == BG) {
        cmdline[len] = ' ';
        cmdline[len + 1] = '&';
        cmdline[len + 2] = 0;
    } else {
        temp = strrchr(cmdline, '&');
        if (temp)
            *temp = 0;
    }
}
/* $end functions related to job */

/* $begin signal handlers */
void sigint_handler(int sig) {
    int                 olderrno = errno;
    pid_t               pid;
    struct list_item    *job_target;

    if (!fg_flag) {
        Sio_puts("\nCSE4100-SP-P2> ");
        return;
    }
    job_target = get_last_job();
    pid = job_target->pid;
    if (pid == getpgrp())
        Kill(-pid, SIGINT);
    else
        Kill(pid, SIGINT);
    remove_job(job_target);
    if (pipe_flag)
        return;
    Sio_puts("\n");

    errno = olderrno;
}

void sigchld_handler(int sig) {
    int                 olderrno = errno;
    int                 status;
    pid_t               pid;
    struct list_item    *job_target;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            job_target = get_job_by_pid(pid);
            job_target->condition = DONE;
        }
    }
    errno = olderrno;
}

void sigtstp_handler(int sig) {
    int                 olderrno = errno;
    pid_t               pid;
    struct list_item    *job_target;

    if (!fg_flag)
        return;
    
    job_target = get_last_job();
    pid = job_target->pid;
    toggle_ampersand(job_target->cmdline, STOP);
    if (pid == getpgrp())
        Kill(-pid, SIGTSTP);
    else
        Kill(pid, SIGTSTP);
    job_target->condition = STOP;
    fg_flag = 0;
    Sio_puts("\n");
    errno = olderrno;
}
/* $end signal handlers */

// fg sigchld는 waitpid
// sigchld handler는 bg만