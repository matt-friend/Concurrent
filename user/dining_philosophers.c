#include "dining_philosophers.h"
#include "libc.h"

uint32_t *forks[PHILOSOPHERS];
bool phils[PHILOSOPHERS];
uint32_t next;

// pseudo-random generator for philosopher wait times
uint32_t random() {
          next = next*1103515245 +12345;
          return (uint32_t) (next*65536)%4294967296;
}

// seed the generator variable
void seed_random(uint32_t seed) {
	next = seed;
}

void philosopher(int p) {
	while(true) {
		// random wait time
		for (volatile int i = 0; i < random(); i++) { 
			asm volatile ( "nop \n" : : : );
		}
		
		// assign left and right forks for a philosopher
		int left = p;
		int right = (p+1)%PHILOSOPHERS;

		// always pick up the lowest index fork first (prevents deadlock)
		if (left < right) {
			sem_wait(forks[left]);
			sem_wait(forks[right]);
		}
		else {
			sem_wait(forks[right]);
			sem_wait(forks[left]);
		}
		
		// print philosopher id, assign true to philosopher index
		char* x;
		itoa(x,p);
        write(0,x,2);
		phils[p] = true;

		// random eat time
		for (volatile int i = 0; i < random(); i++) { 
			asm volatile ( "nop \n" : : : );
		}

		// put down forks
		sem_post(forks[left]);
		sem_post(forks[right]);
		phils[p] = false;
	}
}
			
void main_philosopher() {
	seed_random(1);
	for(int i = 0; i < PHILOSOPHERS; i++) {
		phils[i] = false;
	}

	// initialise all forks (semaphore with value 1 aka mutex)
	for(int i = 0; i < PHILOSOPHERS; i++) {
		forks[i] = sem_init(1);
	}
    // initialise philosopher child processes
	for(int i = 0; i < PHILOSOPHERS; i++) {
		if (0 == fork()) {
			philosopher(i);
			break;
		}
	}
	exit(EXIT_SUCCESS);	
}
			
