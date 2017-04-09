//
//  requestHandlers.h
//  Assignment2-ProxyServer
//
//  Created by Hassaan on 07/04/2017.
//  Copyright © 2017 HaigaTech. All rights reserved.
//

#ifndef requestHandlers_h
#define requestHandlers_h

#include <time.h>


//****************** HANDLE GET REQUEST Via SHARED MEMORY function ******************//

static void handle_get_with_shared_memory (int connection_fd, const char* proxyBaseUrl, const char* page)
{
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
        //        strcpy(path, ROOT);
        //        strcpy(&path[strlen(ROOT)], page);
        
        if ( (fd=open(file_name, O_RDONLY))!=-1 )    //FILE FOUND
        {
            write (connection_fd, ok_response, strlen (ok_response));
            while ( (bytes_read=read(fd, data_to_send, BYTES-1))>0 )
                write (connection_fd, data_to_send, bytes_read);
        }
        else {
            /* Either the requested page was malformed, or we couldn't open a
             module with the indicated name.  Either way, return the HTTP
             response 404, Not Found.  */
            char response[1024];
            
            /* Generate the response message.  */
            snprintf (response, sizeof (response), not_found_response_template, page);
            /* Send it to the client.  */
            //            write (connection_fd, response, strlen (response));
            
            // *********
            
            key_t key;
            int   shmid;
            struct SharedMemory* segptr;
            
//            char charId[30];
//            sprintf(charId, "%d", connection_fd+1000);
            /* Create unique key via call to ftok() */
            key = ftok("/Users/Hassaan/Desktop/ftok.txt", connection_fd);
//            key = ftok(FTOK_KEY, 'S');
            
            if((shmid = shmget(key, SEGMENT_SIZE, IPC_CREAT|IPC_EXCL|0666)) == -1) {
                printf("Shared memory segment exists - opening as client\n");
                
                /* Segment probably already exists - try as a client */
                if((shmid = shmget(key, SEGMENT_SIZE, 0)) == -1) {
                    perror("shmget");
                    exit(1);
                }
            }
            else {
                printf("Creating new shared memory segment\n");
            }
            /* Attach (map) the shared memory segment into the current process */
            (segptr = (struct SharedMemory *)shmat(shmid, 0, 0));
            if( segptr == (struct SharedMemory*)-1) {
                perror("shmat() attaching error");
                exit(1);
            }
            
            if (pthread_mutexattr_init(&segptr->mutexAttr) != 0) {
                perror("pthread_mutexattr_init() error");
            }
            if (pthread_mutexattr_setpshared(&segptr->mutexAttr, PTHREAD_PROCESS_SHARED) != 0) {
                perror("pthread_mutexattr_setpshared() error");
            }
            if (pthread_condattr_init(&segptr->condAttr) != 0) {
                perror("pthread_condattr_init error ");
            }
            if (pthread_condattr_setpshared(&segptr->condAttr, PTHREAD_PROCESS_SHARED) != 0) {
                perror("pthread_condattr_setpshared() error");
            }
            if (pthread_cond_init(&segptr->cond, &segptr->condAttr) != 0) {
                perror("pthread_cond_init() error");
            }
            if (pthread_mutex_init(&segptr->mutex, &segptr->mutexAttr) != 0) {
                perror("pthread_mutexattr_init() error");
            }
            if (pthread_mutex_lock(&segptr->mutex) != 0) {
                perror("pthread_mutex_lock() error");
            }
            

//            strcpy(segptr->data, "Hello shared memory !");
            
            
            //***********
            
            ///// ***********
            
            const char *hostname = proxyBaseUrl;
            struct hostent *hp;
            struct sockaddr_in addr;
            int on = 1;
            int sd; // Socket descriptor that would be used to communicate with the server
            
            if((hp = gethostbyname(hostname)) == NULL){
                herror("gethostbyname");
                exit(1);
            }
            printf("%s\n",hp->h_name );
            bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
            addr.sin_port = htons(SERVER_PORT_NO);
            addr.sin_family = AF_INET;
            
            sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
            
            if(sd == -1){
                perror("setsockopt");
                return;
            }
            if(connect(sd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
                perror("error on connect... \nSkipping");
                shutdown(sd, SHUT_RDWR);
                close(sd);
                return;
            }
            
            char buffer[BUFFER_SIZE];
            char completeRequest[1024] = "LOCAL-GET /index.html HTTP/1.0";
            
            strcat(completeRequest, "\r\n&key=");
            char stringId[20];
            snprintf(stringId, 20, "%d", key);

            strcat(completeRequest, stringId);
            strcat(completeRequest, "\r\n\r\n");
            
            write(sd, completeRequest, strlen(completeRequest));
            bzero(buffer, BUFFER_SIZE);
            
//            long totalBytesRead = 0;
//            long bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
//            while(bytesRead != 0 && bytesRead != -1){
//                write (connection_fd, buffer, BUFFER_SIZE - 1); // send data back to client
//                totalBytesRead += bytesRead;
//                //                    fprintf(stderr, "%s", buffer);
//                bzero(buffer, BUFFER_SIZE);
//                bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
//            }
//            
//            printf("Total bytes received : %ld\n", totalBytesRead);
            
            shutdown(sd, SHUT_RDWR);
            close(sd);
            
            struct timeval tv;
            struct timespec ts;
            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec + 2;
            ts.tv_nsec = 0;
            if (pthread_cond_timedwait(&segptr->cond, &segptr->mutex, &ts) != 0) {
                perror("pthread_cond_timedwait() error");
            }
            send(connection_fd, segptr->data, strlen(segptr->data), 0); // read once the data is written

            pthread_mutex_unlock(&segptr->mutex);
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

            
            //**********
        }
        
    }
    
}
//****************** HANDLE GET REQUEST function ******************//

static void handle_get_with_sockets (int connection_fd, const char* proxyBaseUrl, const char* page)
{
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
        //        strcpy(path, ROOT);
        //        strcpy(&path[strlen(ROOT)], page);
        
        if ( (fd=open(file_name, O_RDONLY))!=-1 )    //FILE FOUND
        {
            write (connection_fd, ok_response, strlen (ok_response));
            while ( (bytes_read=read(fd, data_to_send, BYTES-1))>0 )
                write (connection_fd, data_to_send, bytes_read);
        }
        else {
            /* Either the requested page was malformed, or we couldn't open a
             module with the indicated name.  Either way, return the HTTP
             response 404, Not Found.  */
            char response[1024];
            
            /* Generate the response message.  */
            snprintf (response, sizeof (response), not_found_response_template, page);
            /* Send it to the client.  */
            //            write (connection_fd, response, strlen (response));
            
            
            ///// ***********
            
            const char *hostname = proxyBaseUrl;
            struct hostent *hp;
            struct sockaddr_in addr;
            int on = 1;
            int sd; // Socket descriptor that would be used to communicate with the server
            
            if((hp = gethostbyname(hostname)) == NULL){
                herror("gethostbyname");
                exit(1);
            }
            printf("%s\n",hp->h_name );
            bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
            addr.sin_port = htons(SERVER_PORT_NO);
            addr.sin_family = AF_INET;
            
            sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
            
            if(sd == -1){
                perror("setsockopt");
                return;
            }
            if(connect(sd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
                perror("error on connect... \nSkipping");
                shutdown(sd, SHUT_RDWR);
                close(sd);
                return;
            }
            
            char buffer[BUFFER_SIZE];
            char completeRequest[1024] = "GET /index.html HTTP/1.0\r\n\r\n";
            
            write(sd, completeRequest, strlen(completeRequest));
            bzero(buffer, BUFFER_SIZE);
            
            long totalBytesRead = 0;
            long bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
            while(bytesRead != 0 && bytesRead != -1){
                write (connection_fd, buffer, BUFFER_SIZE - 1); // send data back to client
                totalBytesRead += bytesRead;
                //                    fprintf(stderr, "%s", buffer);
                bzero(buffer, BUFFER_SIZE);
                bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
            }
            
            printf("Total bytes received : %ld\n", totalBytesRead);
            
            shutdown(sd, SHUT_RDWR);
            close(sd);
            
            
            //**********
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
            char proxyAddress[sizeof (buffer)];
            
            /* Some data was read successfully.  NUL-terminate the buffer so
             we can use string operations on it.  */
            buffer[bytes_read] = '\0';
            /* The first line the client sends is the HTTP request, which is
             composed of a method, the requested page, and the protocol
             version.  */
            sscanf (buffer, "%s %s %s %s", method, url, proxyAddress, protocol);
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
            if ((strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) &&
                (strcmp (proxyAddress, "HTTP/1.0") && strcmp (proxyAddress, "HTTP/1.1"))) {
                /* We don't understand this protocol.  Report a bad response.  */
                char response[1024];
                
                /* Generate the response message.  */
                snprintf (response, sizeof (response), bad_request_response, url);
                /* Send it to the client.  */
                //            write (connection_fd, response, strlen (response));
                write (new_sd, response,
                       sizeof(response));
            }
            else if (strcmp (method, "GET")) {
                /* This server only implements the GET method.  The client
                 specified some other method, so report the failure.  */
                char response[1024];
                
                snprintf (response, sizeof (response),
                          bad_method_response_template, method);
                write (new_sd, response, strlen (response));
            }
            else {
                /* A valid request.  Process it.  */
                char baseURL[256] = "127.0.0.1";
                char filepath[256] = "/index.html";
                if (SHOULD_USE_SHARED_MEMORY) {
                    if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                        handle_get_with_shared_memory(new_sd, baseURL, url);
                    }
                    else {
                        sscanf (proxyAddress, "%254[^'/']%s", baseURL, filepath);
                        handle_get_with_shared_memory(new_sd, baseURL, filepath);
                    }
                }
                else {
                    if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                        handle_get_with_sockets(new_sd, baseURL, url);
                    }
                    else {
                        sscanf (proxyAddress, "%254[^'/']%s", baseURL, filepath);
                        handle_get_with_sockets(new_sd, baseURL, filepath);
                    }
                }
            }
        }
        else if (bytes_read == 0)
        /* The client closed the connection before sending any data.
         Nothing to do.  */
            ;
        else
        /* The call to read failed.  */
            perror ("read");
        
        // *******************
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


#endif /* requestHandlers_h */
