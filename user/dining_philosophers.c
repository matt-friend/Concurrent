#include "dining_philosophers.h"
#include "libc.h"

uint32_t *forks[PHILOSOPHERS];
bool phils[PHILOSOPHERS];

void philosopher(int p) {
	while(true) {
		for (volatile int i = 0; i < ((rand() % 100000)*1000); i++) { }

		int left = (p-1)%PHILOSOPHERS;
		int right = p;

		if (left < right) {
			sem_wait(forks[left]);
			sem_wait(forks[right]);
		}
		else {
			sem_wait(forks[right]);
			sem_wait(forks[left]);
		}
		
		char* x;
		itoa(x,p);
        write(0,x,2);
		phils[p] = true;

		for (volatile int i = 0; i < ((rand() % 100000)*1000); i++) { }

		sem_post(forks[left]);
		sem_post(forks[right]);
		phils[p] = false;
	}
}
			


void main_philosopher() {
	for(int i = 0; i < PHILOSOPHERS; i++) {
		phils[i] = false;
	}
	for(int i = 0; i < PHILOSOPHERS; i++) {
		forks[i] = sem_init(1);
	}
	for(int i = 0; i < PHILOSOPHERS; i++) {
		if (0 == fork()) {
			philosopher(i);
			break;
		}
	}
	
}
			
