//
//  helpers.h
//  Assignment2-ProxyServer
//
//  Created by Hassaan on 07/04/2017.
//  Copyright © 2017 HaigaTech. All rights reserved.
//

#ifndef helpers_h
#define helpers_h

#include "dataStructures.h"



// MARK: - Helper Funcitons
//****************** HELPER FUNCTIONS ******************//

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
void parse_command_line_arguments(int argc, char* argv[] )
{
    //Parsing the command line arguments
    if( argc == 2 ) {
        SHOULD_USE_SHARED_MEMORY = atoi(argv[1]);
    }
    else {
        fprintf(stderr, "Usage : %s <Should-use-shared-memory-optimization>\n", argv[0]);
        fprintf(stderr, "Example Usage : %s 1\n", argv[0]);
        exit(1);
    }
}


int createFtokFileIfNotExists(char* fileName)
{
    char completeFileName[1024] = "/Users/Hassaan/Desktop/ftokFiles/file";
    strcat(completeFileName, fileName);
    strcat(completeFileName, ".dat");
    
    strcpy(fileName, completeFileName);
    FILE *fd = fopen(completeFileName, "a");
    if (fd == NULL) {
        perror("Could not open file.");
        return -1;
    }
    else {
        fclose(fd);
    }
    return 0;
}

#endif /* helpers_h */
