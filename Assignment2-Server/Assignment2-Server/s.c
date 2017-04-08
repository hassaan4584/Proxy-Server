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
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/ipc.h>
#include <sys/shm.h>


// MARK: - Constants
#define PROXY_SERVER_PORT_NO            5568
#define SERVER_PORT_NO					PROXY_SERVER_PORT_NO + 100
#define QUIT 							"QUIT"
#define NEW_CONNECTION			 		"NEW-CONNECTION"
#define FILE_SUCCESSFULLY_RECEIVED		"FILE-SUCCESSFULLY-RECEIVED"
#define BYTES                           1024
#define MAX_THREAD_COUNT                1024
#define BUFFER_SIZE                     1024
#define SEGMENT_SIZE                    4*1024
#define FTOK_KEY                        "/Hassaan"

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

struct SharedMemory
{
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    bool hasFileCompletelyWritten;
    char data[BUFFER_SIZE];
};

void *handle_request(void *param);
void send_file(char *fileName, int new_sd);
int createThreadPool(struct ThreadPoolManager *manager);

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
	my_addr.sin_port = htons(SERVER_PORT_NO);	//assigning the port
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
	}

	return 0;
}
//****************** HANDLE LOCAL GET REQUEST function ******************//

static void handle_local_get (int connection_fd, const char* page)
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
            
            key_t key;
            int   shmid;
            struct SharedMemory* segptr;
            
            /* Create unique key via call to ftok() */
            key = ftok(FTOK_KEY, 'S');
            
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
                perror("shmat");
                exit(1);
            }
            
            strcpy(segptr->data, response);

            /// ****************
            
            //            size_t n;
            //            int sockfd, portno, flag;
            //            struct sockaddr_in serv_addr;
            //            struct in_addr *pptr;
            //            struct hostent *server;
            //
            //            char buffer[256];
            //            /* Extract host and port from HTTP Request */
            //            char hoststring[100] = "www.google.com";
            //            char req[1024] = "GET http://www.google.com.pk/?gws_rd=cr&amp;ei=iqDkWPukEIGMsgHboaaIBw HTTP/1.0\r\n\r\n";
            //            /* Parsing the request string */
            ////            for(i=0; i<strlen(req); i++)
            ////            {
            ////                if(req[i] == 'H' && req[i+1] == 'o' && req[i+2] == 's' && req[i+3] == 't')
            ////                {
            ////                    for(j=i+6; req[j] != '\r'; j++)
            ////                    {
            ////                        hoststring[j-i-6] = req[j];
            ////                    }
            ////                    hoststring[j] = '\0';
            ////                    break;
            ////                }
            ////            }
            ////            printf("\nHost extracted : '%s'\n", hoststring);
            //            /* default port */
            //            portno = 80;
            //
            //            /* Create a socket point */
            //            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            //            if(sockfd < 0)
            //            {
            //                perror("Error opening socket\n");
            //                exit(1);
            //            }
            //            server = gethostbyname(hoststring);
            //            if (server == NULL)
            //            {
            //                fprintf(stderr, "No such host\n");
            //                exit(0);
            //            }
            ////            printf("\nConnected to host\n");
            //
            //            bzero((char *) &serv_addr, sizeof(serv_addr));
            //            serv_addr.sin_family = AF_INET;
            //            pptr = (struct in_addr  *)server->h_addr;
            //            bcopy((char *)pptr, (char *)&serv_addr.sin_addr, server->h_length);
            //            serv_addr.sin_port = htons(portno);
            //
            //            /* Connect to server */
            //            if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
            //            {
            //                perror("Error in connecting to server\n");
            //                exit(1);
            //            }
            //
            //            /* Message to be sent to the server */
            //            /* printf("message to server : "); */
            //            bzero(buffer, 256);
            //            /* fgets(buffer, 255, stdin); */
            //
            //            /* Send message to server */
            //            n = write(sockfd, req, strlen(req));
            //            if(n == 0) {
            //                perror("Error writing to socket\n");
            //                exit(1);
            //            }
            //            
            //            /* Read server response */
            //            bzero(buffer, 256);
            //            flag = 1;
            //            size_t i;
            ////            printf("\nreading server response\n");
            //            while( (n = read(sockfd, buffer, 255) > 0))
            //            {
            //                if(flag)
            //                {
            //                    printf("%s", buffer);
            //                    flag = 0;
            //                }
            //                i = write(connection_fd, buffer, strlen(buffer));
            //            }
            //            close(sockfd);
            
            
            //// ******************
        }
        
        /* Try to open the module.  */
        //        module = module_open (module_file_name);
    }
    
}

//****************** HANDLE GET REQUEST function ******************//

static void handle_get (int connection_fd, const char* page)
{
    printf("\nServer. Transmitting data via sockets.");
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
            
            /// ****************
            
//            size_t n;
//            int sockfd, portno, flag;
//            struct sockaddr_in serv_addr;
//            struct in_addr *pptr;
//            struct hostent *server;
//            
//            char buffer[256];
//            /* Extract host and port from HTTP Request */
//            char hoststring[100] = "www.google.com";
//            char req[1024] = "GET http://www.google.com.pk/?gws_rd=cr&amp;ei=iqDkWPukEIGMsgHboaaIBw HTTP/1.0\r\n\r\n";
//            /* Parsing the request string */
////            for(i=0; i<strlen(req); i++)
////            {
////                if(req[i] == 'H' && req[i+1] == 'o' && req[i+2] == 's' && req[i+3] == 't')
////                {
////                    for(j=i+6; req[j] != '\r'; j++)
////                    {
////                        hoststring[j-i-6] = req[j];
////                    }
////                    hoststring[j] = '\0';
////                    break;
////                }
////            }
////            printf("\nHost extracted : '%s'\n", hoststring);
//            /* default port */
//            portno = 80;
//            
//            /* Create a socket point */
//            sockfd = socket(AF_INET, SOCK_STREAM, 0);
//            if(sockfd < 0)
//            {
//                perror("Error opening socket\n");
//                exit(1);
//            }
//            server = gethostbyname(hoststring);
//            if (server == NULL)
//            {
//                fprintf(stderr, "No such host\n");
//                exit(0);
//            }
////            printf("\nConnected to host\n");
//            
//            bzero((char *) &serv_addr, sizeof(serv_addr));
//            serv_addr.sin_family = AF_INET;
//            pptr = (struct in_addr  *)server->h_addr;
//            bcopy((char *)pptr, (char *)&serv_addr.sin_addr, server->h_length);
//            serv_addr.sin_port = htons(portno);
//            
//            /* Connect to server */
//            if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
//            {
//                perror("Error in connecting to server\n");
//                exit(1);
//            }
//            
//            /* Message to be sent to the server */
//            /* printf("message to server : "); */
//            bzero(buffer, 256);
//            /* fgets(buffer, 255, stdin); */
//            
//            /* Send message to server */
//            n = write(sockfd, req, strlen(req));
//            if(n == 0) {
//                perror("Error writing to socket\n");
//                exit(1);
//            }
//            
//            /* Read server response */
//            bzero(buffer, 256);
//            flag = 1;
//            size_t i;
////            printf("\nreading server response\n");
//            while( (n = read(sockfd, buffer, 255) > 0))
//            {
//                if(flag)
//                {
//                    printf("%s", buffer);
//                    flag = 0;
//                }
//                i = write(connection_fd, buffer, strlen(buffer));
//            }
//            close(sockfd);
            
            
            //// ******************
        }
        
        /* Try to open the module.  */
//        module = module_open (module_file_name);
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

	if (pthread_cond_wait(&cond[threadDetails->threadNumber], &mutex[threadDetails->threadNumber]) != 0) {
	    perror("pthread_cond_timedwait() error");
	}

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
            if (strcmp(method, "LOCAL-GET") == 0) {
                handle_local_get(new_sd, url);
            }
            else {
                handle_get (new_sd, url);
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




