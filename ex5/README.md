alonmarko208, idanorg
Idan Orgad 206281388, Alon Markovich 313454902

FILES:
container.cpp - container implementation
sockets.cpp - client server communication implementation
Makefile
README


Part 1: Theoretical Questions

1.One use is to manage what father’s processes are hidden from the son.
It means that processes in one namespace pid can not see processes 
from another namespace pid. 
2.Mount namespaces is the set of filesystem mounts that are visible to
a process.It provides isolation of the list of mounts seen by the 
processes in each namespace instance.
3.One use of CGroups is to define what a container can do, 
i.e what limitations it has. 
4.Fork creates a new process by duplicating the calling process. 
The new process is referred to as the child process. 
The calling process is referred to as the parent process. 
Clone creates a new child process, in a manner similar to fork but it 
has more control over what pieces of execution context are shared 
between the calling process and the child process
5.Chroot changes the root - it gets a pointer to a path, which is now 
the new root directory.
6.Proc file system presents information about processes that are 
currently running, and used as information center for kernel. 
It provides a method of communication between kernel space and user 
space. For example (via web) the GNU version of the process reporting 
utility ps uses the proc file system to obtain its data, without using 
any specialized system calls. 

