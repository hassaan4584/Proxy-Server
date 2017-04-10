//
//  s.c
//  Assignment2-Server
//
//  Created by Hassaan on 07/04/2017.
//  Copyright © 2017 HaigaTech. All rights reserved.
//
// For creating the response, some help and code has been taken
// from the following link : advancedlinuxprogramming.com/listings/chapter-11/server.c
// For implementing queue, library has been taken
// from the following link : https://github.com/ClickerMonkey/CDSL

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
#include<fcntl.h>
#include<signal.h>
#include <netinet/tcp.h>
#include<netdb.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/ipc.h>
#include <sys/shm.h>
#include "dataStructures.h"
#include "response.h"
#include "queue.h"

#define SERVER_DIRECTORY_PATH       "/Volumes/Parhai/Parhai/LUMS/Semester 2 Spring17/Advanced Operating Systems/Assignments/Assignment 2/Assignment2-Server/Assignment2-Server/"


int createThreadPool(struct ThreadPoolManager *manager);
int getNextEmptyThreadNumber(struct ThreadPoolManager* poolManager, int nextThreadNumber, bool force) {
    
    if (poolManager->threadArr[nextThreadNumber].isFree == true) {
        return nextThreadNumber; // the next thread is free
    }
    for (int i=(nextThreadNumber+1)%MAX_THREAD_COUNT ; i != nextThreadNumber ; i = (i+1)%MAX_THREAD_COUNT) {
        if (poolManager->threadArr[i].isFree == true) {
            return i;
        }
    }
    sleep(1);
    if (force) {
        return getNextEmptyThreadNumber(poolManager, 1, false);
    }
    else {
        return -1;
    }
    
}

int send_data_using_shared_memory(const char *ftokFileName, const char* threadNoAsKey, const char* data, size_t length) {
    
    return 0;
}


// MARK: - Main Function
//****************** MAIN FUNCTION ******************//

int main()
{
    waitingRequestsQueue = newQueue();
	int threadCount = 0;
	struct ThreadPoolManager tm;
    ROOT = getenv("PWD");
	int s_id;
	struct sockaddr_in my_addr, remote_addr;
	s_id = socket(PF_INET, SOCK_STREAM, 0);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(SERVER_PORT_NO);	//assigning the port
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");	//assigning the self ip address

	if(bind(s_id, (struct sockaddr*)&my_addr, sizeof(my_addr) ) == -1) {
		error("Server failed to bind\n");
        exit(2);
	}
    if (listen(s_id,2) == -1) {
        perror("Server. Could not listen");
    }
	unsigned int size = sizeof (struct sockaddr_in);

	int new_sd=0;
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

// MARK: - Handle Local Get
//****************** HANDLE LOCAL GET REQUEST function ******************//

static void handle_local_get (int connection_fd, const char* page, const char* ftokFileName, const char* threadNoAsKey)
{
    char data_to_send[BYTES+1];
    int fd;
    long bytes_read;
    /* Make sure the requested page begins with a slash and does not
     contain any additional slashes -- we don't support any
     subdirectories.  */
    if (*page == '/' && strchr (page + 1, '/') == NULL) {
        
        // If the request is valid we attach the passed shared memory segment
        key_t intKey;
        int   shmid;
        struct SharedMemory* segptr;
        /* Create unique key via call to ftok() */
        intKey = ftok(ftokFileName, atoi(threadNoAsKey));
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
        if (pthread_cond_init(&segptr->cond_server, &segptr->condAttr) != 0) {
            perror("Server. pthread_cond_init() error");
        }


        
        char file_name[1024];
        
        if ( strncmp(page, "/\0", 2)==0 ) {
            snprintf (file_name, sizeof ("index.html"), "index.html");
            //Because if no file is specified, index.html will be opened by default
        }
        else {
            snprintf (file_name, sizeof (file_name), "%s", page);
            // removing the "/" on the first index
            memmove (file_name, file_name+1, strlen (file_name+1) + 1);
        }

        char filePath[1024];
        strcpy(filePath, SERVER_DIRECTORY_PATH);
        strcat(filePath, file_name);
        fd = open(file_name, O_RDONLY); //if file opens, server running from terminal
        if (fd == -1) {
            fd = open(filePath, O_RDONLY); //if file opens, server running using xcode
        }
        if ( fd !=-1 )
        {
            write (connection_fd, ok_response, strlen (ok_response));
            send_data_using_shared_memory(ftokFileName, threadNoAsKey, ok_response, strlen (ok_response));
            strncpy(segptr->data, ok_response, strlen (ok_response));
            segptr->dataWritten = strlen(ok_response);

            if (pthread_mutex_lock(&segptr->mutex) != 0) {
                perror("Server. pthread_mutex_lock() error");
            }
            if (pthread_cond_broadcast(&segptr->cond_ps) != 0) {
                perror("Server. pthread_cond_broadcast() error");
            }
            while ( (bytes_read=read(fd, data_to_send, BYTES-1))>0 ) {
                send_data_using_shared_memory(ftokFileName, threadNoAsKey, data_to_send, bytes_read);
                strncpy(segptr->data, data_to_send, bytes_read);
                segptr->dataWritten = bytes_read;
                segptr->hasFileCompletelyWritten = false;
                if (pthread_cond_broadcast(&segptr->cond_ps) != 0) {
                    perror("Server. pthread_cond_broadcast() error");
                }
                if (pthread_cond_wait(&segptr->cond_server, &segptr->mutex) != 0) {
                    perror("Server. pthread_cond_timedwait() error");
                }

            }
            segptr->hasFileCompletelyWritten = true;
            if (pthread_mutex_unlock(&segptr->mutex) != 0) {
                perror("Server. pthread_mutex_lock() error");
            }
            if (pthread_cond_broadcast(&segptr->cond_ps) != 0) {
                perror("Server. pthread_cond_broadcast() error");
            }
            close(fd);
        }
        else {
            /* Either the requested page was malformed, or we couldn't open a
             module with the indicated name.  Either way, return the HTTP
             response 404, Not Found.  */
            char response[1024];
            /* Generate the response message.  */
            snprintf (response, sizeof (response), not_found_response_template, page);
            /* Write it on the shared memory  */
            strcpy(segptr->data, response);
            segptr->dataWritten = sizeof(response);
            segptr->hasFileCompletelyWritten = true;

        }
        
        // Signal Proxy server that all data has been written
        if (pthread_cond_signal(&segptr->cond_ps) != 0) {
            perror("pthread_cond_signal() error");
        }
        // After all the data has been written, we will detach this shared memory segment.
        if (shmdt(segptr) == -1) {
            perror("shmdt. Could not detach");
        }

    }
    
}


// MARK: - Handle Get Request

static void handle_get (int connection_fd, const char* page)
{
    printf("Server. Transmitting data via sockets.\n");
    char data_to_send[BYTES];
    int fd;
    long bytes_read;
    /* Make sure the requested page begins with a slash and does not
     contain any additional slashes -- we don't support any
     subdirectories.  */
    if (*page == '/' && strchr (page + 1, '/') == NULL) {
        char file_name[1024];
        
        
        if ( strncmp(page, "/\0", 2)==0 ) {
            snprintf (file_name, sizeof ("index.html"), "index.html");
            //Because if no file is specified, index.html will be opened by default
        }
        else {
            snprintf (file_name, sizeof (file_name), "%s", page);
            // removing the "/" on the first index
            memmove (file_name, file_name+1, strlen (file_name+1) + 1);
        }
        
        if ( (fd=open(file_name, O_RDONLY))!=-1 )    //FILE FOUND
        {
            write (connection_fd, ok_response, strlen (ok_response));
            while ( (bytes_read=read(fd, data_to_send, BYTES-1))>0 ) {
                write (connection_fd, data_to_send, bytes_read);
            }
        }
        else {
            /* Either the requested page was malformed, or we couldn't open a
             module with the indicated name.  Either way, return the HTTP
             response 404, Not Found.  */
            char response[1024];
            
            /* Generate the response message.  */
            snprintf (response, sizeof (response), not_found_response_template, page);
            /* Send it to the client.  */
            write (connection_fd, response, strlen (response));
            printf("Total Response sent : %ld\n", strlen(response));
            
        }
        
    }
    
}

//****************** WORKER THREAD handle_request function ******************//

void *handle_request(void *param)
{
	struct Thread *threadDetails = (struct Thread*)param;
	if(threadDetails == NULL) {
	printf("\nTerminating worker thread \n");
		return NULL;
	}
    
    while (true) {
        threadDetails->isFree = true;
        if (pthread_cond_wait(&cond[threadDetails->threadNumber], &mutex[threadDetails->threadNumber]) != 0) {
            perror("pthread_cond_timedwait() error");
        }
    ReloadPendingRequest:
        threadDetails->isFree = false;
        
        int new_sd=threadDetails->socketId;
        char welcome[1024] = "***** Worker thread number : ";
        printf("%s %d  *****\n", welcome, threadDetails->threadNumber);
        
        
        ///*******************
        
        char buffer[256];
        ssize_t bytes_read;
        
        /* Read some data from the client.  */
        bytes_read = read (new_sd, buffer, sizeof (buffer) - 1);
        if (bytes_read > 0) {
            char method[sizeof (buffer)];
            char url[sizeof (buffer)];
            char protocol[sizeof (buffer)];
            
            /* Some data was read successfully.  NUL-terminate the buffer so
             we can use string operations on it.  */
            buffer[bytes_read] = '\0';
            /* The first line the client sends is the HTTP request, which is
             composed of a method, the requested page, and the protocol
             version.  */
            sscanf (buffer, "%s %s %s", method, url, protocol);
            /* The client may send various header information following the
             request.  For this HTTP implementation, we don't care about it.
             However, we need to read any data the client tries to send.  Keep
             on reading data until we get to the end of the header, which is
             delimited by a blank line.  HTTP specifies CR/LF as the line
             delimiter.  */
            while (strstr (buffer, "\r\n\r\n") == NULL)
                bytes_read = read (new_sd, buffer, sizeof (buffer));
            /* Make sure the last read didn't fail.  If it did, there's a
             problem with the connection, so give up.  */
            if (bytes_read == -1) {
                close (new_sd);
                return NULL;
            }
            /* Check the protocol field.  We understand HTTP versions 1.0 and
             1.1.  */
            if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                /* We don't understand this protocol.  Report a bad response.  */
                write (new_sd, bad_request_response,
                       sizeof (bad_request_response));
            }
            else if (strcmp (method, "GET") && strcmp(method, "LOCAL-GET")) {
                /* This server only implements the GET method.  The client
                 specified some other method, so report the failure.  */
                char response[1024];
                
                snprintf (response, sizeof (response),
                          bad_method_response_template, method);
                write (new_sd, response, strlen (response));
            }
            else {
                /* A valid request.  Process it.  */
                char temp[256] ;
                char ftokFileName[1024] ;
                char threadNoAsKey[20];
                if (strcmp(method, "LOCAL-GET") == 0) {
                    // if its a request from a local proxy server, we ll use shared memory to pass data between the two processses.
                     // extracting key from the recceived request
                    sscanf(buffer, "%254[^'=']=%s %254[^'=']=%s", temp, ftokFileName, temp, threadNoAsKey);
                    handle_local_get(new_sd, url, ftokFileName, threadNoAsKey);

                }
                else {
                    handle_get (new_sd, url);
                }
            }
        }
        else if (bytes_read == 0) {
            /* The client closed the connection before sending any data.
             Nothing to do.  */
        }
        else
        /* The call to read failed.  */
            perror ("read failed");
        
        shutdown(new_sd, SHUT_RDWR);
        close(new_sd);
    
        if (!queue_isEmpty(waitingRequestsQueue)) {
            int *socketID = (int*) queue_poll(waitingRequestsQueue);
            threadDetails->socketId = *socketID;
            goto ReloadPendingRequest;
        }
        else {
            if (pthread_mutex_init(&mutex[threadDetails->threadNumber], NULL) != 0) {
                perror("pthread_mutex_init() error");
            }
            if (pthread_cond_init(&cond[threadDetails->threadNumber], NULL) != 0) {
                perror("pthread_cond_init() error");
            }
            if (pthread_mutex_lock(&mutex[threadDetails->threadNumber]) != 0) {
                perror("pthread_mutex_lock() error");
            }
        }
  
    }


	printf("Terminating worker thread no : %d\n\n", threadDetails->threadNumber);
	return NULL;
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




