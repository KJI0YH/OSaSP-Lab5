#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define CHILDS_COUNT 2

//thread IDs
pthread_t tid[CHILDS_COUNT+1];

//get current time
void getTime(int ordChild){
	struct timeval tv;
	struct timezone tz;
	
	if (gettimeofday(&tv, &tz) == -1)
		perror("Can not get current time\n");
	else {
	
		int mls = tv.tv_usec / 1000;
		int ss = tv.tv_sec % 60;
		int mm = (tv.tv_sec / 60) % 60;
		int hh = (tv.tv_sec / 3600 + 3) % 24;
	
		if (ordChild == 0)
			printf("Parent ");
		else
			printf("Child %d ", ordChild);
		
		printf("ID: %ld PID: %d PPID: %d Time: %02d:%02d:%02d:%03d\n\n", tid[ordChild], getpid(), getppid(), hh, mm, ss, mls);	
		fflush(stdout);
	}
}

//routine for a thread
void* start_routine(void *arg) {

	pthread_t id = pthread_self();
	
	for (int i = 1; i <= CHILDS_COUNT; i++) {
		if (pthread_equal(id, tid[i])) {
			getTime(i);
			break;
		}
	}
	
	pthread_exit(NULL);
}

void main(void) {
	
	//parent ID
	tid[0] = pthread_self();
	getTime(0);
	
	//creating threads
	for (int i = 1; i <= CHILDS_COUNT; i++) {
		if (pthread_create(&(tid[i]), NULL, &start_routine, NULL)) {
			perror("Can not to start a thread");
			tid[i] = 0;
		}
	}
	
	//join threads
	for (int i = 1; i <= CHILDS_COUNT; i++) 
		if (pthread_join(tid[i], NULL) != 0)	
			perror("Can not join a thread");
	
	return;
}
