#include "dining_philosophers.h"
#include "libc.h"

uint32_t *forks[PHILOSOPHERS];

void philosopher(int p) {
	while(true) {
		for (int i = 0; i < rand(); i++) { continue; }

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
		
		for (int i = 0; i < rand(); i++) { continue; }

		sem_post(forks[left]);
		sem_post(forks[right]);
	}
}
			


void main_philosopher() {
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
			
