ABOUT:

This program simulates cars entering and exiting a 4-way stop sign intersection using semaphores and mutexes in C++'s pthread.h and semaphore.h.
Cars are designed to appear at the same direction at the same time between executions, but can be developed further to "randomize" entry points and times.


HOW TO RUN:

This program was tested in Docker's ubuntu container environment. The command sequence used to compile and run the code is as follows: 
	
	docker restart xv6-projects
	docker attach xv6-projects
	cd ./xv6
	g++ tc.cpp -lpthread -lrt -fpermissive -o tc
	./tc

KNOWN ISSUES:

- Time does not record decimal values of compilation.
