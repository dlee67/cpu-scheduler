This program does the following.
1) Create handlers for two signals.
2) Create an idle process which will be executed when there is nothing
   else to do.
3) Create a send_signals process that sends a SIGALRM every so often

When run, it should produce the following output (approximately):

```
state:        3
name:         IDLE
pid:          4493
ppid:         4491
interrupts:   0
switches:     0
started:      0
In ISR stopped:   4493
---- entering scheduler
continuing  4493
---- leaving scheduler
In ISR stopped:   4493
---- entering scheduler
continuing  4493
---- leaving scheduler
Terminated: 15
```

We will be adding the following functionality.
1) Change the NUM_SECONDS to 20.

2) Take any number of arguments for executables, and place each on the
    processes list with a state of NEW. These executables will not require
    arguments themselves.

3) When a SIGALRM arrives, scheduler() will be called. Currently, it runs
    the only process in the system, the idle process. Instead, do the
    following.
    a)  Update the PCB for the process that was interrupted including the
        number of context switches and interrupts it had, and changing its
        state from RUNNING to READY.
    b)  If there is a process with a NEW status on the processes list, change
        its state to RUNNING, and fork() and execl() it.
    c)  If there are no NEW processes, round robin between the READY
        processes, not including the idle process.  If no process is READY
        in the processes list, execute the idle process.

4) When a SIGCHLD arrives notifying that a child has exited, process_done() is
    called. process_done() currently only prints out the PID and the status.
    a)  Add the printing of the information in the PCB including the number
        of times it was interrupted, the number of times it was context
        switched (this may be fewer than the interrupts if a process
        becomes the only non-idle process in the ready queue), and the total
        system time the process took.
    b)  Change the state to TERMINATED.
    c)  Start the idle process to use the rest of the time slice.
