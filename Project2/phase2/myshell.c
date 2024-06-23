/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
void    eval(char *cmdline);
void    run_pipe(char **argv, int fd);
char    **get_next_argv(char **argv);
int     parseline(char *buf, char **argv);
int     builtin_command(char **argv); 
void    builtin_cd(char *path);
void    sigint_handler(int sig);
void    sigchld_handler(int sig);
void    sigtstp_handler(int sig);

volatile int            fg_flag;
volatile int            pipe_flag;
volatile sig_atomic_t   pid_atomic;

int main(void) {
    char    cmdline[MAXLINE]; /* Command line */

    Signal(SIGINT, sigint_handler);
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGTSTP, sigtstp_handler);

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
    char        *argv[MAXARGS]; /* Argument list execve() */
    char        buf[MAXLINE];   /* Holds modified command line */
    int         bg;             /* Should the job run in bg or fg? */
    pid_t       pid;            /* Process id */

    sigset_t    mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    /* Ignore empty lines */
    if (argv[0] == NULL)  
	    return;

    if (builtin_command(argv))
        return;
    
    if (!bg) { // Foreground job
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if ((pid = Fork()) == 0) { // child process
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
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
            pid_atomic = 0;
            while (!pid_atomic)
                Sigsuspend(&prev_one);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            pipe_flag = 0;
            fg_flag = 0;
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
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);
        if ((pid = Fork()) == 0) { // child
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
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
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            pid_atomic = 0;
            while (!pid_atomic)
                Sigsuspend(&prev_one);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            run_pipe(argv_next, pipefd[0]);
        }
    }
}

/* Get next argv (command), for checking pipe */
char **get_next_argv(char **argv) {
    while (*argv)
        argv++;
    
    return argv + 1;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
    if (!strcmp(argv[0], "exit")) {
	    exit(0);
    } /* quit command */

    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    
    if (!strcmp(argv[0], "cd")) {
        builtin_cd(argv[1]);
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

/* $begin signal handlers */
void sigint_handler(int sig) {
    int     olderrno = errno;

    if (pipe_flag)
        return;
    if (!fg_flag)
        Sio_puts("\nCSE4100-SP-P2> ");
    else {
        Sio_puts("\n");
    }

    errno = olderrno;
}

void sigchld_handler(int sig) {
    int olderrno = errno;
    int status;

    pid_atomic = waitpid(-1, &status, WUNTRACED);
    errno = olderrno;
}

void sigtstp_handler(int sig) {
    int olderrno = errno;

    Sio_puts("\n");
    errno = olderrno;
}
/* $end signal handlers */