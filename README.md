# P2_COP4610
Project 2 for Operating Systems
Project Members: Chris Caballero, John Mijares, Anika Patel

The TAR File Contains:

/Part1/empty.c | empty program implementation
/Part1/empty.trace | obtained from strace, records system calls called
/Part1/part1.c | implementation four system calls to the program
/Part1/part1.trace | obtained from strace, records system calls called

/Part2/my_timer.c | kernel module implementation
/Part2/Makefile | contains commands to compile and make executable for kernel module
  /usr/src/hello > sudo make | compiles the file + creates kernel object my_timer.ko
  make clean | removes the kernel object and cleans up unnecessary files

/Part3/elevator.c | elevator implementation
/Part3/sys_call.c | syscall implementation
/Part3/Makefile | contains commands to compile and make executable elevator file
  make | compiles the kernel module + compiles a new syscall implementation
  make clean | removes the kernal module and cleans up unnecessary files

README.md | docmentation of project and distribution of labor

Known Bugs + Unfinished Portions: N/A

Division of Labor:

Part 1: System-call Tracing | Chris Caballero
Part 2: Kernel Module | John Mijares, Anika Patel, Chris Caballero
Part 3-1: Kernel Module with an Elevator | John Mijares, Anika Patel, Chris Caballero
Part 3-2: Add System Calls | Chris Caballero & Anika Patel
Part 3-3: /Proc | John Mijares, Anika Patel, Chris Caballero
Part 3-4: Test | Chris Caballero
