/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout); //清空缓冲区并输出
	fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
*/
void eval(char *cmdline)
{
    /* $begin handout */
    char *argv[MAXARGS]; /* argv for execve() */
    int bg;              /* should the job run in bg or fg? */
    pid_t pid;           /* process id */
    sigset_t mask;       /* signal mask */

    /* Parse command line */
    bg = parseline(cmdline, argv);
    if (argv[0] == NULL)
	return;   /* ignore empty lines */

    if (!builtin_cmd(argv)) {

        /*
	 * This is a little tricky. Block SIGCHLD, SIGINT, and SIGTSTP
	 * signals until we can add the job to the job list. This
	 * eliminates some nasty races between adding a job to the job
	 * list and the arrival of SIGCHLD, SIGINT, and SIGTSTP signals.
	 */

	if (sigemptyset(&mask) < 0)
	    unix_error("sigemptyset error");
	if (sigaddset(&mask, SIGCHLD))
	    unix_error("sigaddset error");
	if (sigaddset(&mask, SIGINT))
	    unix_error("sigaddset error");
	if (sigaddset(&mask, SIGTSTP))
	    unix_error("sigaddset error");
	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
	    unix_error("sigprocmask error");

	/* Create a child process */
	if ((pid = fork()) < 0)
	    unix_error("fork error");

	/*
	 * Child  process
	 */

	if (pid == 0) {
	    /* Child unblocks signals 解除阻塞*/
	    sigprocmask(SIG_UNBLOCK, &mask, NULL);

	    /* Each new job must get a new process group ID
	       so that the kernel doesn't send ctrl-c and ctrl-z
	       signals to all of the shell's jobs */
	    if (setpgid(0, 0) < 0)
		unix_error("setpgid error");

	    /* Now load and run the program in the new job */
	    if (execve(argv[0], argv, environ) < 0) {
		printf("%s: Command not found\n", argv[0]);
		exit(0);
	    }
	}

	/*
	 * Parent process
	 */

	/* Parent adds the job, and then unblocks signals so that
	   the signals handlers can run again */
	addjob(jobs, pid, (bg == 1 ? BG : FG), cmdline);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	if (!bg)
	    waitfg(pid);
	else
	    printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
    }
    /* $end handout */
    return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;

    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
   builtin_cmd  - 如果用户键入了内置命令，则立即执行。
 */
int builtin_cmd(char **argv)
{
    sigset_t mask,prev_mask;
    sigfillset(&mask);
    if(!strcmp(argv[0],"quit"))   //退出命令
        exit(0);
    else if(!strcmp(argv[0],"&"))     //忽略单独的&
        return 1;
    else if(!strcmp(argv[0],"jobs"))  //输出作业列表中所有作业信息
    {
        sigprocmask(SIG_BLOCK,&mask,&prev_mask); //访问全局变量，阻塞所有信号
        listjobs(jobs);
        sigprocmask(SIG_SETMASK,&prev_mask,NULL);
        return 1;
    }
    else if(!strcmp(argv[0],"bg")||!strcmp(argv[0],"fg"))  //实现bg和fg两条内置命令
    {
        do_bgfg(argv);
        return 1;
    }
    return 0;     /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
    执行内置bg和fg命令
 */
void do_bgfg(char **argv)
{
    /* $begin handout */
    struct job_t *jobp=NULL;

    /* Ignore command if no argument
    如果没有参数，则忽略命令*/
    if (argv[1] == NULL) {
	printf("%s command requires PID or %%jobid argument\n", argv[0]);
	return;
    }

    /* Parse the required PID or %JID arg */
    if (isdigit(argv[1][0])) //判断串1的第0位是否为数字
    {
	pid_t pid = atoi(argv[1]);  //atoi把字符串转化为整型数
	if (!(jobp = getjobpid(jobs, pid))) {
	    printf("(%d): No such process\n", pid);
	    return;
	}
    }
    else if (argv[1][0] == '%') {
	int jid = atoi(&argv[1][1]);
	if (!(jobp = getjobjid(jobs, jid))) {
	    printf("%s: No such job\n", argv[1]);
	    return;
	}
    }
    else {
	printf("%s: argument must be a PID or %%jobid\n", argv[0]);
	return;
    }

    /* bg command */
    if (!strcmp(argv[0], "bg")) {
	if (kill(-(jobp->pid), SIGCONT) < 0)
	    unix_error("kill (bg) error");
	jobp->state = BG;
	printf("[%d] (%d) %s", jobp->jid, jobp->pid, jobp->cmdline);
    }

    /* fg command */
    else if (!strcmp(argv[0], "fg")) {
	if (kill(-(jobp->pid), SIGCONT) < 0)
	    unix_error("kill (fg) error");
	jobp->state = FG;
	waitfg(jobp->pid);
    }
    else {
	printf("do_bgfg: Internal error\n");
	exit(0);
    }
    /* $end handout */
    return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) //传入的是一个前台进程的pid
{
    sigset_t mask;
    sigemptyset(&mask);  //初始化mask为空集合
    while(pid==fgpid(jobs))
    {
        sigsuspend(&mask);  //暂时挂起
    }
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 sigchld_handler  - 只要子作业终止（变为僵尸），
 内核就会向shell发送SIGCHLD，或者因为收到SIGSTOP或SIGTSTP信号而停止。
 处理程序收集所有可用的僵尸子节点，但不等待任何其他当前正在运行的子节点终止。
 */
void sigchld_handler(int sig)
{
    struct job_t *job1;   //新建结构体
    int olderrno = errno,status;
    sigset_t mask, prev_mask;
    pid_t pid;
    sigfillset(&mask);
    while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0)
    {
        /*尽可能回收子进程，WNOHANG | WUNTRACED表示立即返回，
        如果等待集合中没有进程被中止或停止返回0，否则孩子返回进程的pid*/
        sigprocmask(SIG_BLOCK,&mask,&prev_mask);  //需要deletejob，阻塞所有信号
        job1 = getjobpid(jobs,pid);  //通过pid找到job
        if(WIFSTOPPED(status)) //子进程停止引起的waitpid函数返回
        {
            job1->state = ST;
            printf("Job [%d] (%d) terminated by signal %d\n",job1->jid,job1->pid,WSTOPSIG(status));
        }
        else
        {
            if(WIFSIGNALED(status)) //子进程终止引起的返回
                printf("Job [%d] (%d) terminated by signal %d\n",job1->jid,job1->pid,WTERMSIG(status));
            deletejob(jobs, pid);  //终止的进程直接回收
        }
        fflush(stdout);
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    }
    errno = olderrno;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
  sigint handler  - 只要用户在键盘上键入ctrl-c，
 内核就会向shell发送一个SIGINT。 抓住它并将其发送到  前台作业。
 */
void sigint_handler(int sig)
{
    pid_t pid ;
    sigset_t mask, prev_mask;
    int olderrno=errno;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK,&mask,&prev_mask);  //需要获取前台进程pid，阻塞所有信号
    pid = fgpid(jobs);  //获取前台作业pid
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    if(pid!=0)  //只处理前台作业
        kill(-pid,SIGINT);
    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 sigtstp_handler  - 只要用户在键盘上键入ctrl-z，
 内核就会向shell发送SIGTSTP。 通过向它发送SIGTSTP来捕获它并暂停前台作业。
 */
void sigtstp_handler(int sig)
{
    pid_t pid;
    sigset_t mask,prev_mask;
    int olderrno = errno;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK,&mask,&prev_mask);  //需要获取前台进程pid，阻塞所有信号
    pid = fgpid(jobs);
    sigprocmask(SIG_SETMASK,&prev_mask,NULL);
    if(pid!=0)
        kill(-pid,SIGTSTP);
    errno = olderrno;
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG:
		    printf("Running ");
		    break;
		case FG:
		    printf("Foreground ");
		    break;
		case ST:
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ",
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



