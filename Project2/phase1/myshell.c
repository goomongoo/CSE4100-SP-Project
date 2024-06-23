/* $begin shellmain */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128

/* Function prototypes */
void    eval(char *cmdline);
int     parseline(char *buf, char **argv);
int     builtin_command(char **argv); 
void    builtin_cd(char *path);
void    sigint_handler(int sig);
void    sigchld_handler(int sig);

volatile int            fg_flag;
volatile sig_atomic_t   pid_atomic;

int main(void) {
    char    cmdline[MAXLINE]; /* Command line */

    Signal(SIGINT, sigint_handler);
    Signal(SIGCHLD, sigchld_handler);

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
    int         status;

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
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        } else { // parent process
            Sigprocmask(SIG_BLOCK, &mask_all, NULL);
            fg_flag = 1;
            pid_atomic = 0;
            while (!pid_atomic)
                Sigsuspend(&prev_one);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            fg_flag = 0;
        }
    }

    return;
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
	    *delim = '\0';
	    buf = delim + 1;
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	    argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

/* $begin signal handlers */
void sigint_handler(int sig) {
    int     olderrno = errno;

    Sio_puts("\n");
    if (!fg_flag)
        Sio_puts("CSE4100-SP-P2> ");

    errno = olderrno;
}

void sigchld_handler(int sig) {
    int olderrno = errno;

    pid_atomic = waitpid(-1, NULL, 0);
    errno = olderrno;
}
/* $end signal handlers */