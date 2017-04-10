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



// MARK: - Server GET Requests with shared memory

static void handle_get_with_shared_memory (int connection_fd, const char* proxyBaseUrl, const char* page, int threadNumber)
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
            
            char ftokFileName[128];
            snprintf(ftokFileName, 127, "%d", threadNumber);
            createFtokFileIfNotExists(ftokFileName);
            /* Create unique key via call to ftok() */
            key = ftok(ftokFileName, threadNumber);
            
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
            if (pthread_cond_init(&segptr->cond_ps, &segptr->condAttr) != 0) {
                perror("pthread_cond_init() error");
            }
            if (pthread_mutex_init(&segptr->mutex, &segptr->mutexAttr) != 0) {
                perror("pthread_mutexattr_init() error");
            }
            if (pthread_mutex_lock(&segptr->mutex) != 0) {
                perror("pthread_mutex_lock() error");
            }
            
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
            char completeRequest[1024] = "LOCAL-GET ";

            strcat(completeRequest, page);
            strcat(completeRequest, " HTTP/1.0");
            
            strcat(completeRequest, "\r\n&fileName=");
            char stringId[1024];
            snprintf(stringId, 1023, "%s", ftokFileName);
            strcat(completeRequest, stringId);

            strcat(completeRequest, "\r\n&threadNumber=");
            snprintf(stringId, 10, "%d", threadNumber);
            
            strcat(completeRequest, stringId);
            strcat(completeRequest, "\r\n\r\n");
            
            write(sd, completeRequest, strlen(completeRequest));
            bzero(buffer, BUFFER_SIZE);
            
            // terminating connection with server because response would be in the shared memory.
            shutdown(sd, SHUT_RDWR);
            close(sd);
            
//            struct timeval tv;
//            struct timespec ts;
//            gettimeofday(&tv, NULL);
//            ts.tv_sec = tv.tv_sec + 2;
//            ts.tv_nsec = 0;
            if (pthread_cond_wait(&segptr->cond_ps, &segptr->mutex) != 0) {
                perror("pthread_cond_timedwait() error");
            }
            send(connection_fd, segptr->data, segptr->dataWritten, 0); // read once the data is written
            pthread_mutex_unlock(&segptr->mutex);
            if (pthread_cond_broadcast(&segptr->cond_server) != 0) {
                perror("Server. pthread_cond_broadcast() error");
            }
            while (segptr->hasFileCompletelyWritten == false) {
                if (pthread_cond_broadcast(&segptr->cond_server) != 0) {
                    perror("Server. pthread_cond_broadcast() error");
                }
                if (pthread_cond_wait(&segptr->cond_ps, &segptr->mutex) != 0) {
                    perror("pthread_cond_timedwait() error");
                }
                send(connection_fd, segptr->data, segptr->dataWritten, 0); // read once the data is written
                pthread_mutex_unlock(&segptr->mutex);
                if (pthread_cond_broadcast(&segptr->cond_server) != 0) {
                    perror("Server. pthread_cond_broadcast() error");
                }
            }
            
//            struct shmid_ds buff;
//            if (shmctl(shmid, IPC_STAT, &buff) == -1) {
//                perror("shmctl() error with IPC_STAT");
//            }
//            if (shmctl(shmid, IPC_RMID, &buff) == -1) // remove the shared memory segment
//            {
//                perror("shmctl() error");
//            }
//            if (shmdt(segptr) == -1) {
//                perror("shmdt() error");
//            }
//            else {
//                printf("Deleting shared memory segment\n");
//            }
        }
        
    }
    
}

// MARK: - Server GET Requests with sockets

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
            
            size_t n;
            int sockfd, portno, flag;
            struct sockaddr_in serv_addr;
            struct in_addr *pptr;
            struct hostent *server;
            
            char buffer[256];
            /* Extract host and port from HTTP Request */
            const char *hoststring = proxyBaseUrl;
//            char req[1024] = "GET http://www.google.com.pk/?gws_rd=cr&amp;ei=iqDkWPukEIGMsgHboaaIBw HTTP/1.0\r\n\r\n";
            char completeRequest[1024] = "GET ";
            if (strcmp(proxyBaseUrl, "127.0.0.1") && strcmp(proxyBaseUrl, "localhost")) {
                // The proxy server and actual server are on the different machine.
                strcat(completeRequest, proxyBaseUrl);
            }
            else {
            }
            strcat(completeRequest, "/");
            strcat(completeRequest, file_name);
            strcat(completeRequest, " HTTP/1.0");
            strcat(completeRequest, "\r\n\r\n");

            if (strcmp(proxyBaseUrl, "127.0.0.1") && strcmp(proxyBaseUrl, "localhost"))
                portno = 80; // Port where webservers are listening to the requests
            else
                portno = SERVER_PORT_NO;
            /* Create a socket point */
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if(sockfd < 0)
            {
                perror("Error opening socket\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                return;
            }
            server = gethostbyname(hoststring);
            if (server == NULL)
            {
                fprintf(stderr, "No such host\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                return;
            }
            
            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            pptr = (struct in_addr  *)server->h_addr;
            bcopy((char *)pptr, (char *)&serv_addr.sin_addr, server->h_length);
            serv_addr.sin_port = htons(portno);
            
            /* Connect to server */
            if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
            {
                perror("Error in connecting to server\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                return;
            }
            
            /* Message to be sent to the server */
            /* printf("message to server : "); */
            bzero(buffer, 256);
            /* fgets(buffer, 255, stdin); */
            
            /* Send message to server */
            n = write(sockfd, completeRequest, strlen(completeRequest));
            if(n == 0) {
                perror("Error writing to socket\n");
                exit(1);
            }
            
            /* Read server response */
            bzero(buffer, 256);
            flag = 1;
            size_t i;
            //            printf("\nreading server response\n");
            while( (n = read(sockfd, buffer, 255) > 0))
            {
                if(flag)
                {
                    printf("%s", buffer);
                    flag = 0;
                }
                i = write(connection_fd, buffer, strlen(buffer));
            }
            close(sockfd);
        }
    }
}


// MARK: - Handle Incoming Requests

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
                    /* Extracting required information from the get request */
                    sscanf (proxyAddress, "%254[^'/']%s", baseURL, filepath);
                    if (strcmp(baseURL, "HTTP") == 0) {
                        //if the request came from firefox
                        strcpy(baseURL, "127.0.0.1");
                        strcpy(filepath, "/index.html");
                    }
                    
                    if (strcmp(baseURL, "127.0.0.1") && strcmp(baseURL, "localhost")) {
                        /* If the actual server is not the present at 127.0.0.1, then even if we have the
                         shared memory optimization enabled, we will still make the request to the actual
                         server using sockets. Since this proxy server and the actual servers are present
                         on different machines */
                        
                        /* Differntiating if the request came from an actual web browser or
                         from my own web client */
                        if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                            handle_get_with_sockets(new_sd, baseURL, url);
                        }
                        else {
                            sscanf (proxyAddress, "%254[^'/']%s", baseURL, filepath);
                            handle_get_with_sockets(new_sd, baseURL, filepath);
                        }
                    }
                    else {
                        /* If the actual server and this proxy server are running on one machine. */
                        if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                            handle_get_with_shared_memory(new_sd, baseURL, url, threadDetails->threadNumber);
                        }
                        else {
                            handle_get_with_shared_memory(new_sd, baseURL, filepath, threadDetails->threadNumber);
                        }
                    }
                }
                else {
                    /* If the shared memory optimization is disabled, we will use sockets 
                     for forwarding requests. */
                    if (strcmp (protocol, "HTTP/1.0") && strcmp (protocol, "HTTP/1.1")) {
                        handle_get_with_sockets(new_sd, baseURL, url);
                    }
                    else {
                        sscanf (proxyAddress, "%254[^'/']%s/", baseURL, filepath);
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
