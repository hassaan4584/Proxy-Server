// For creating the response, some help and code has been taken
// from the following link : advancedlinuxprogramming.com/listings/chapter-11/server.c


#ifdef __APPLE__
#  define error printf
#endif

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/ipc.h>
#include <sys/shm.h>
#include "response.h"
#include "helpers.h"
#include "dataStructures.h"
#include "requestHandlers.h"
#include "queue.h"




int createThreadPool(struct ThreadPoolManager *manager);
void ex_program(int sig);



// MARK: - Main Function
//****************** MAIN FUNCTION ******************//

int main(int argc, char* argv[] )
{
    (void) signal(SIGINT, ex_program);
    waitingRequestsQueue = newQueue();
    parse_command_line_arguments(argc, argv);
	int threadCount = 0;
    ROOT = getenv("PWD");
	int s_id;
	struct sockaddr_in my_addr, remote_addr;
	s_id = socket(PF_INET, SOCK_STREAM, 0);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(PROXY_SERVER_PORT_NO);	//assigning the port
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");	//assigning the self ip address

	if(bind(s_id, (struct sockaddr*)&my_addr, sizeof(my_addr) ) == -1) {
		error("Server failed to bind\n");
        exit(2);
	}
	listen(s_id,2); 
	unsigned int size = sizeof (struct sockaddr_in);

	int new_sd=0;
    struct ThreadPoolManager tm;
	createThreadPool(&tm);

	while(1)
	{
        // accepting the client's request and assigning new socket
        new_sd = accept (s_id, (struct sockaddr *) &remote_addr, &size);
        if(new_sd == -1) {
            error("Server. Could not accept the request");
            continue;
        }
        if ((threadCount = getNextEmptyThreadNumber(&tm, threadCount, true)) == -1) {
            perror("Unable to accept request");
            queue_offer(waitingRequestsQueue, &new_sd);
            continue;
        }
        tm.threadArr[threadCount].socketId = new_sd;
        if (pthread_cond_signal(&cond[threadCount]) != 0) {
            perror("pthread_cond_signal() error");
        }
		threadCount++;
        if (threadCount >= MAX_THREAD_COUNT) {
            threadCount = 0;
        }
	}

	return 0;
}

//****************** THREAD MANAGEMENT ******************//

int createThreadPool(struct ThreadPoolManager *manager)
{
	for(int i=0 ; i < MAX_THREAD_COUNT ; i++)
	{
		if (pthread_mutex_init(&mutex[i], NULL) != 0) {
		    perror("pthread_mutex_init() error");
		}
		if (pthread_cond_init(&cond[i], NULL) != 0) {
			perror("pthread_cond_init() error");
		}
		if (pthread_mutex_lock(&mutex[i]) != 0) {
			perror("pthread_mutex_lock() error");
		}
	}
  	pthread_attr_t attr;
	pthread_attr_init(&attr); // Required!!!
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	struct Thread *threadPool = manager->threadArr;
	threadPool[0].threadId = 0;

	for(int i=0 ; i < MAX_THREAD_COUNT ; i++) {
		threadPool[i].threadNumber = i;
		if (pthread_create(&threadPool[i].threadId, &attr, handle_request, (void*) &threadPool[i]) != 0) {
			printf("Cannot create %d th thread\n", i+1);
		}
		else {
			printf("Created %d th thread\n", i+1);
		}
	}

	return 0;

}

// MARK: - Ctrl C
void ex_program(int sig) {
    printf("Catched signal: %d ... !!\n", sig);
    
    for (int i=0 ; i< MAX_THREAD_COUNT ; i++) {
        
        key_t intKey;
        int   shmid;
        struct SharedMemory* segptr;
        /* Create unique key via call to ftok() */
        char ftokFileName[128];
        snprintf(ftokFileName, 127, "%d", i);
        intKey = ftok(ftokFileName, i);
        /* Segment probably already exists - try as a client */
        if((shmid = shmget(intKey, SEGMENT_SIZE, 0)) == -1) {
            perror("shmget");
            return;
        }
        /* Attach (map) the shared memory segment into the current process */
        (segptr = (struct SharedMemory *)shmat(shmid, 0, 0));
        if( segptr == (struct SharedMemory*)-1) {
            perror("shmat");
            return;
        }

        struct shmid_ds buff;
        if (shmctl(shmid, IPC_STAT, &buff) == -1) {
            perror("shmctl() error with IPC_STAT");
        }
        if (shmctl(shmid, IPC_RMID, &buff) == -1) // remove the shared memory segment
        {
            perror("shmctl() error");
        }
        if (shmdt(segptr) == -1) {
            perror("shmdt() error");
        }
        else {
            printf("Deleting shared memory segment\n");
        }
        
    } //  end for
    
    (void) signal(SIGINT, SIG_DFL);
}


