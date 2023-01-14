idanorg, alonmarko208
Idan Orgad 206281388, Alon Markovich 313454902
EX: 2


FILES:
Makefile -- A given exercise file from the project.
uthreads.cpp-- a library that manages user level threads - has built in classes (Handler and Thread). 
README -- A brief description to the exercise.


REMARKS:
none


ANSWERS:


1. one example is sorting an array with mergeSort. In every recursive call / iteration we split the
array and we do not want to save the data and use the kernel for that. In this case we will not involve the kernel,
and it is not even aware of the existence of the threads.
2. One of the advantages is that every tab is managed separately,
and each tab has its own resources. It means that whenever one tab fails and has to be closed,
it does not influence other tabs. They are independent. 
Moreover, the processes can run in parallel on different processors. 
One of the disadvantages is that there are many processes to handle,
the operations require a kernel trap which causes significant work.
3. keyboard interrupts occur when we write the command,
after we submit that specific command in the shell "kill pid" it sends a signal (SIGINT)
that kills the application involuntarily. 
4. Real time is the time that operations are executed.
An example can be the clock in the computer. virtual time is used to measure computational progress
(and to define synchronization), and an example may be the measurement time we had in ex1. 
5. sigsetjmp - If the return is from a successful direct invocation, sigsetjmp() returns 0.
If the return is from a call to siglongjmp(), sigsetjmp() returns a non-zero value.
siglongjmp - After siglongjmp() is completed, program execution continues as
if the corresponding invocation of sigsetjmp() had just returned the value specified by val. 
The siglongjmp() function cannot cause sigsetjmp() to return 0; if val is 0, sigsetjmp() returns the value 1.
