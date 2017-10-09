#include <iostream>
#include <list>
#include <iterator>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>

#define NUM_SECONDS 20

using namespace std;

/* Write a string */
#define WRITES(a) { assert (write (STDOUT_FILENO, a, strlen (a)) >= 0); }
/* Write an integer i no longer than l */
#define WRITEI(i,l) { char buf[l]; assert (eye2eh (i, buf, l, 10) != -1); WRITES (buf); }
/* Write a newline */
#define WRITENL { assert (write (STDOUT_FILENO, "\n", 1) == 1); }

enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED };

struct PCB
{
    STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
};

PCB *running;

// http://www.cplusplus.com/reference/list/list/
list<PCB *> processes;

int sys_time;

/*
** a signal handler for those signals delivered to this process, but
** not already handled.
*/
void grab (int signum) { assert (false); }

// c++decl> declare ISV as array 32 of pointer to function (int) returning
// void
void (*ISV[32])(int) = {
/*        00    01    02    03    04    05    06    07    08    09 */
/*  0 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 10 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 20 */ grab, grab, grab, grab, grab, grab, grab, grab, grab, grab,
/* 30 */ grab, grab
};

// http://man7.org/linux/man-pages/man7/signal-safety.7.html

/*
** Async-safe integer to a string. i is assumed to be positive. The number
** of characters converted is returned; -1 will be returned if bufsize is
** less than one or if the string isn't long enough to hold the entire
** number. Numbers are right justified. The base must be between 2 and 16;
** otherwise the string is filled with spaces and -1 is returned.
*/
int eye2eh (int i, char *buf, int bufsize, int base)
{
    if (bufsize < 1) return (-1);
    buf[bufsize-1] = '\0';
    if (bufsize == 1) return (0);
    if (base < 2 || base > 16)
    {
        for (int j = bufsize-2; j >= 0; j--)
        {
            buf[j] = ' ';
        }
        return (-1);
    }

    int count = 0;
    const char *digits = "0123456789ABCDEF";
    for (int j = bufsize-2; j >= 0; j--)
    {
        if (i == 0)
        {
            buf[j] = ' ';
        }
        else
        {
            buf[j] = digits[i%base];
            i = i/base;
            count++;
        }
    }
    if (i != 0) return (-1);
    return (count);
}

/*
** an overloaded output operator that prints a PCB
*/
ostream& operator << (ostream &os, struct PCB *pcb)
{
    os << "state:        " << pcb->state << endl;
    os << "name:         " << pcb->name << endl;
    os << "pid:          " << pcb->pid << endl;
    os << "ppid:         " << pcb->ppid << endl;
    os << "interrupts:   " << pcb->interrupts << endl;
    os << "switches:     " << pcb->switches << endl;
    os << "started:      " << pcb->started << endl;
    return (os);
}

/*
** an overloaded output operator that prints a list of PCBs
*/
ostream& operator << (ostream &os, list<PCB *> which) {
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = which.begin(); PCB_iter != which.end(); PCB_iter++) {
        os << (*PCB_iter);
    }
    return (os);
}

/*
**  send signal to process pid every interval for number of times.
*/
void send_signals (int signal, int pid, int interval, int number)
{
    for (int i = 1; i <= number; i++)
    {
        assert (sleep (interval) == 0);

        if (kill (pid, signal) == -1)
        {
            perror ("kill");
            return;
        }
    }
}

struct sigaction *create_handler (int signum, void (*handler)(int))
{
    struct sigaction *action = new (struct sigaction);

    action->sa_handler = handler;
/*
**  SA_NOCLDSTOP
**  If  signum  is  SIGCHLD, do not receive notification when
**  child processes stop (i.e., when child processes  receive
**  one of SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU).
*/
    if (signum == SIGCHLD)
    {
        action->sa_flags = SA_NOCLDSTOP;
    }
    else
    {
        action->sa_flags = 0;
    }

    assert (sigemptyset (&(action->sa_mask)) == 0);
    assert (sigaction (signum, action, NULL) == 0);
    return (action);
}

int fork_proc(const char* name){
    //create a child process
    pid_t fork_return = 0;
    fork_return = fork();

    // here we check the return value of the fork, -1, 0, or >0
    // an error ocurred
    if(fork_return < 0){
        perror("No fork: An error occured!");
        exit(errno);
    }
        // we are the child process
    else if(fork_return == 0){
        //execute process
        execl(name, name, "0", NULL);
    }
    // else we are the parent process
    return EXIT_SUCCESS;
}

void scheduler (int signum)
{
    WRITES ("---- entering scheduler\n");
    assert (signum == SIGALRM);
    sys_time++;

    PCB* tocont = processes.front();

    WRITES ("continuing");
    WRITEI (tocont->pid, 7);
    WRITENL;

    // update interrupted process
    tocont->state = READY;
    tocont->interrupts = tocont->interrupts++;
    tocont->switches = tocont->switches++;

    // loop through process list looking for NEW procs
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = processes.begin(); PCB_iter != processes.end(); PCB_iter++)
    {
        // if we find a process of status NEW
        if((*PCB_iter)->state == NEW)
        {
            // change it to RUNNING and fork and execl it
            (*PCB_iter)->state = RUNNING;
            fork_proc((*PCB_iter)->name);
        }
    }

    // loop though process list looking for READY procs
    for (PCB_iter = processes.begin(); PCB_iter != processes.end(); PCB_iter++)
    {
        // if we find a process of status RUNNING which is not the idle process
        if((*PCB_iter)->state == READY && std::string((*PCB_iter)->name) != std::string("IDLE"))
        {
            // change it to RUNNING and fork and execl it
            (*PCB_iter)->state = RUNNING;
            fork_proc((*PCB_iter)->name);
        }
    }

    // loop though process list looking for the idle proc
    for (PCB_iter = processes.begin(); PCB_iter != processes.end(); PCB_iter++)
    {
        // if we find the idle process
        if(std::string((*PCB_iter)->name) == std::string("IDLE"))
        {
            // change it to RUNNING
            (*PCB_iter)->state = RUNNING;
            break;
        }
    }

    if (kill (tocont->pid, SIGCONT) == -1)
    {
        WRITES ("in scheduler kill error: ");
        WRITEI (errno, 7);
        WRITENL;
        return;
    }
    WRITES ("---- leaving scheduler\n");
}

void process_done (int signum)
{
    WRITES ("---- entering process_done\n");
    cout << processes;
    assert (signum == SIGCHLD);
    PCB* to_terminate = running;

    for (;;)
    {
        int status, cpid;
        cpid = waitpid (-1, &status, WNOHANG);

        if (cpid < 0)
        {
            WRITES (sys_errlist[errno]);
            WRITENL;
        }
        else if (cpid == 0)
        {
            WRITES ("cpid == 0\n");
            break;
        }
        else
        {
            WRITES ("process exited: ");
            WRITEI (cpid, 7);
            WRITENL;
        }
    }

    // set the current processes state to TERMINATED
    to_terminate->state = TERMINATED;

    // loop though process list looking for the idle proc, and set it as running
    list<PCB *>::iterator PCB_iter;
    for (PCB_iter = processes.begin(); PCB_iter != processes.end(); PCB_iter++)
    {
        // if we find the idle process
        if(std::string((*PCB_iter)->name) == std::string("IDLE"))
        {
            // change it to RUNNING
            (*PCB_iter)->state = RUNNING;
            break;
        }
    }

    WRITES ("---- leaving process_done\n");
}

/*
** stop the running process and index into the ISV to call the ISR
*/
void ISR (int signum)
{
    if (signum != SIGCHLD)
    {
        if (kill (running->pid, SIGSTOP) == -1)
        {
            WRITES ("In ISR kill returned: ");
            WRITEI (errno, 7);
            WRITENL;
            return;
        }

        WRITES ("In ISR stopped: ");
        WRITEI (running->pid, 7);
        WRITENL;
        running->state = READY;
    }

    ISV[signum](signum);
}

void start_clock()
{
    int ret;
    if ((ret = fork()) == 0)
    {
        // signal this process once a second for three times
        send_signals (SIGALRM, getppid(), 1, NUM_SECONDS);

        // once that's done, really kill everything...
        kill (0, SIGTERM);
    }

    assert (ret > 0);
}

void create_idle ()
{
    PCB* idle = new (PCB);
    idle->state = READY;
    idle->name = "IDLE";
    idle->ppid = getpid();
    idle->interrupts = 0;
    idle->switches = 0;
    idle->started = sys_time;

    if ((idle->pid = fork()) == 0)
    {
        // the pause might be interrupted, so we need to
        // repeat it forever.
        for (;;)
        {
            pause();
            if (errno == EINTR) { continue; }
            perror ("pause");
        }
    }

    processes.push_back (idle);
    running = idle;
}

void create_proc (char *proc_name)
{
    PCB* new_proc = new (PCB);
    new_proc->state = NEW;
    new_proc->name = proc_name;
    new_proc->ppid = getpid();
    new_proc->interrupts = 0;
    new_proc->switches = 0;
    new_proc->started = sys_time;

    processes.push_back (new_proc);
}

int main (int argc, char **argv)
{
    sys_time = 0;
    int args_len = sizeof(argv);
    ISV[SIGALRM] = scheduler;       create_handler (SIGALRM, ISR);
    ISV[SIGCHLD] = process_done;    create_handler (SIGCHLD, ISR);


    // create a process to soak up cycles
    create_idle ();

    // create a process for each argument
    // 1. create func create_arg_procs
    //    for each arg init proc (w/ state NEW), add process to proc list
    for(int i=1;i<args_len;i++)
    {
        create_proc(argv[i]);
    }
    //cout << processes;

    cout << running;

    start_clock();

    // we keep this process around so that the children don't die and
    // to keep the IRQs in place.
    for (;;)
    {
        pause();
        if (errno == EINTR) { continue; }
        perror ("pause");
    }
}
