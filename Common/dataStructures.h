//
//  dataStructures.h
//  Assignment2-ProxyServer
//
//  Created by Hassaan on 07/04/2017.
//  Copyright © 2017 HaigaTech. All rights reserved.
//

#ifndef dataStructures_h
#define dataStructures_h



// MARK: - Constants

#define PROXY_SERVER_PORT_NO			5558
#define SERVER_PORT_NO					PROXY_SERVER_PORT_NO + 100
#define QUIT 							"QUIT"
#define NEW_CONNECTION			 		"NEW-CONNECTION"
#define FILE_SUCCESSFULLY_RECEIVED		"FILE-SUCCESSFULLY-RECEIVED"
#define LOCAL_GET                       "LOCAL-GET"
#define BYTES                           1024
#define MAX_THREAD_COUNT                1024
#define BUFFER_SIZE                     1024
#define SEGMENT_SIZE                    10
#define FTOK_KEY                        "/Hassaan"

// MARK: - Global Variables

bool SHOULD_USE_SHARED_MEMORY = false;
pthread_cond_t cond[MAX_THREAD_COUNT];
pthread_mutex_t mutex[MAX_THREAD_COUNT];
char *ROOT;



// MARK: - Data Structures


struct Thread
{
    pthread_t threadId;
    int threadNumber;
    bool isFree;
    int socketId; // client say iss socketID pay rabta krna hay
};

struct ThreadPoolManager
{
    struct Thread threadArr[MAX_THREAD_COUNT];
    int totalThreadCount;
};

struct SharedMemory
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_condattr_t condAttr;
    pthread_mutexattr_t mutexAttr;
    bool hasFileCompletelyWritten;
    char data[BUFFER_SIZE];
};

#endif /* dataStructures_h */
