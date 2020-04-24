#include "dining_philosophers.h"

int *forks[PHILOSOPHERS];

void main_philosopher() {
	for(int i = 0; i < PHILOSOPHERS; i++) {
		forks[i] = sem_init();
	}
	for(int i = 0; i < PHILOSOPHERS, i++) {
		if (fork()) {
			philospher(i);
			
