# P2_COP4610
## Project Members: 
### Chris Caballero, John Mijares, Anika Patel

## The TAR File Contains:
#### Part 1 
    (1) /Part1/empty.c | empty program implementation
    (2) /Part1/empty.trace | obtained from strace, records system calls called
    (3) /Part1/part1.c | adds four system calls to the empty program
    (4) /Part1/part1.trace | obtained from strace, records system calls called
#### Part 2
    (5) /Part2/my_timer.c | kernel module implementation
    (6) /Part2/Makefile | contains commands to compile and make executable for kernel module
        (a) creates module and related files in the current working directory
    (7) sudo make | compiles the file + creates kernel object my_timer.ko
    (8) make clean | removes the kernel object and cleans up unnecessary files
#### Part 3
    (1) /Part3/elevator.c | elevator implementation
        (a) contains structs for the elevator thread, building, floors and passengers
        (b) contains proc file functions and module init + module exit
    (2) /Part3/sys_call.c | syscall implementation for:
        (a) start_elevator, issue_request, and stop_elevator
    (3) /Part3/Makefile | contains commands to compile and make executable elevator file
    (4) make | compiles the kernel module + compiles a new syscall implementation
    (5) make clean | removes the kernal module and cleans up unnecessary files

#### README.md 
    (1) README.md | docmentation of project and distribution of labor

## Known Bugs + Unfinished Portions + Considerations:
    N/A

## Division of Labor:

    Part 1: System-call Tracing | Chris Caballero
    Part 2: Kernel Module | John Mijares, Anika Patel, Chris Caballero
    Part 3-1: Kernel Module with an Elevator | John Mijares, Anika Patel, Chris Caballero
    Part 3-2: Add System Calls | Chris Caballero & Anika Patel
    Part 3-3: /Proc | Anika Patel, Chris Caballero
    Part 3-4: Test | Chris Caballero, John Mijares
    README: Anika Patel, Chris Caballero
    Documentation - Parts 1, 2: John Mijares
    Documentation - Part 3: Chris Caballero
