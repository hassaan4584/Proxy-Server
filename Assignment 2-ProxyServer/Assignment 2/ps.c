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
#include<fcntl.h>
#include<signal.h>
#include <netinet/tcp.h>
#include<netdb.h>


// MARK: - Constants

#define PROXY_SERVER_PORT_NO			5569
#define SERVER_PORT_NO					PROXY_SERVER_PORT_NO + 100
#define QUIT 							"QUIT"
#define NEW_CONNECTION			 		"NEW-CONNECTION"
#define FILE_SUCCESSFULLY_RECEIVED		"FILE-SUCCESSFULLY-RECEIVED"
#define BYTES                           1024
#define MAX_THREAD_COUNT                1024
#define BUFFER_SIZE                     1024

pthread_cond_t cond[MAX_THREAD_COUNT];
pthread_mutex_t mutex[MAX_THREAD_COUNT];
char *ROOT;

/* HTTP response and header for a successful request.  */

static char* ok_response =
"HTTP/1.0 200 OK\n"
"Content-Type: text/html\n"
"\n";
//"<html>\n"
//" <body>\n"
//"  <h1>Server</h1>\n"
//"  <p>This is the response from the server.</p>\n"
//" </body>\n"
//"</html>\n";

/* HTTP response, header, and body indicating that the we didn't
 understand the request.  */

static char* bad_request_response =
"HTTP/1.0 400 Bad Request\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Bad Request</h1>\n"
"  <p>This server did not understand your request.</p>\n"
" </body>\n"
"</html>\n";

/* HTTP response, header, and body template indicating that the
 requested document was not found.  */

static char* not_found_response_template =
"HTTP/1.0 404 Not Found\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Not Found</h1>\n"
"  <p>The requested URL %s was not found on this server.</p>\n"
" </body>\n"
"</html>\n";

/* HTTP response, header, and body template indicating that the
 method was not understood.  */

static char* bad_method_response_template =
"HTTP/1.0 501 Method Not Implemented\n"
"Content-type: text/html\n"
"\n"
"<html>\n"
" <body>\n"
"  <h1>Method Not Implemented</h1>\n"
"  <p>The method %s is not implemented by this server.</p>\n"
" </body>\n"
"</html>\n";

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

void *handle_request(void *param);
void send_file(char *fileName, int new_sd);
int createThreadPool(struct ThreadPoolManager *manager);

int getNextEmptyThreadNumber(struct ThreadPoolManager* poolManager, int nextThreadNumber, bool force);
int myReceive(int socket, char *arr, int length, int flag);
int mySend(int socket, char *arr, int length, int flag);


// MARK: - Main Function
//****************** MAIN FUNCTION ******************//

int main()
{
	int threadCount = 0;
	struct ThreadPoolManager tm;
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
	createThreadPool(&tm);

	while(1)
	{
        // accepting the client's request and assigning new socket
        new_sd = accept (s_id, (struct sockaddr *) &remote_addr, &size);
        if(new_sd == -1) {
            error("Server. Could not accept the request");
            continue;
        }
        tm.threadArr[threadCount].socketId = new_sd;
        if (pthread_cond_signal(&cond[threadCount]) != 0) {
            perror("pthread_cond_signal() error");
        }
		threadCount++;
        if (threadCount == MAX_THREAD_COUNT) {
            threadCount = 0;
        }
        threadCount = getNextEmptyThreadNumber(&tm, threadCount, true);
	}

	return 0;
}


//****************** HANDLE GET REQUEST function ******************//

static void handle_get (int connection_fd, const char* page)
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
            
            char hostname[1024] = "127.0.0.1";
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
            else if (strcmp (method, "GET")) {
                /* This server only implements the GET method.  The client
                 specified some other method, so report the failure.  */
                char response[1024];
                
                snprintf (response, sizeof (response),
                          bad_method_response_template, method);
                write (new_sd, response, strlen (response));
            }
            else
            /* A valid request.  Process it.  */
                handle_get (new_sd, url);
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

// MARK: - Helper Funcitons
//****************** HELPER FUNCTIONS ******************//

int getNextEmptyThreadNumber(struct ThreadPoolManager* poolManager, int nextThreadNumber, bool force) {
    
    if (poolManager->threadArr[nextThreadNumber].isFree == true) {
        return nextThreadNumber; // the next thread is free
    }
    for (int i=(nextThreadNumber+1)%MAX_THREAD_COUNT ; i != nextThreadNumber ; i++) {
        if (poolManager->threadArr[i].isFree == true) {
            return i;
        }
    }
    sleep(1);
    if (force) {
        return getNextEmptyThreadNumber(poolManager, 1, false);
    }
    else {
        return 1;
    }
}


