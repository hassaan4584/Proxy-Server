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
#include <netinet/tcp.h>
#include <netdb.h>
#include "dataStructures.h"
#include "response.h"


struct Details
{
    int socket_id;
    int requests_per_thread;
    int thread_number;
};


void parse_command_line_arguments(int argc, char* argv[], int*, int* );
void create_required_threads();
void *make_request(void *param);



// MARK: - Main Function

int main(int argc, char* argv[] )
{
	 system("clear");

	 int totalThreads = 0;
	 int requestsPerThread = 0;

    parse_command_line_arguments(argc, argv, &totalThreads, &requestsPerThread);
    printf("The number of threads : %d\n", totalThreads);
    printf("The number of requests per thread : %d\n", requestsPerThread);

    char hostname[1024] = PROXY_IP_ADDRESS;
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
    addr.sin_port = htons(PROXY_SERVER_PORT_NO);
    addr.sin_family = AF_INET;
    
    pthread_t *allThreads = malloc(sizeof(pthread_t)*totalThreads);
    for (int i=0; i<totalThreads; i++) {
        sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
        
        if(sd == -1){
            perror("setsockopt");
            continue;
        }
        if(connect(sd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
            perror("error on connect... \nSkipping");
            continue;
        }

        struct Details *myDetails = malloc (sizeof(struct Details));
        myDetails->requests_per_thread = requestsPerThread;
        myDetails->socket_id = sd;
        myDetails->thread_number = i;
        pthread_create(&allThreads[i], NULL, make_request, (void*) myDetails);

    }
    
    for (int i=0; i<totalThreads; i++) {
        pthread_join(allThreads[i], NULL);
    }

    printf("Exitting from main thread");

    return 0;
}


// MARK: - Create Requests

void *make_request(void *param)
{
    struct Details *details = (struct Details*)param;
    if (details == NULL) {
        return NULL;
    }
    int sd = details->socket_id;
    char buffer[BUFFER_SIZE];
//    char completeRequest[1024] = "GET /index.html HTTP/1.0\r\n\r\n";
    char completeRequest[1024] = "GET ";
    strcat(completeRequest, PROXY_IP_ADDRESS);
//    strcat(completeRequest, " www.google.com.pk/index.html");
    strcat(completeRequest, " 127.0.0.1/index.html");
    strcat(completeRequest, " HTTP/1.0");
    
    
    strcat(completeRequest, "\r\nUser-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.11; rv:52.0) Gecko/20100101 Firefox/52.0");
    strcat(completeRequest, "\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    strcat(completeRequest, "\r\nConnection: keep-alive");
    strcat(completeRequest, "\r\n\r\n");
    for (int i=0 ; i<details->requests_per_thread ; i++) {
        
        write(sd, completeRequest, strlen(completeRequest));
        bzero(buffer, BUFFER_SIZE);
        
        long totalBytesRead = 0;
        long bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
        while(bytesRead != 0 && bytesRead != -1){
            totalBytesRead += bytesRead;
            fprintf(stderr, "%s", buffer);
            bzero(buffer, BUFFER_SIZE);
            bytesRead = read(sd, buffer, BUFFER_SIZE - 1);
        }
        
        printf("Thread : %d   Request : %d    Total bytes received : %ld\n", details->thread_number, i, totalBytesRead);
        
    }
    shutdown(sd, SHUT_RDWR);
    close(sd);
    
    return NULL;
}

//****************** HELPER FUNCTIONS ******************//

void create_required_threads() 
{
	int s_id;
	struct sockaddr_in serv_addr;
	s_id = socket (PF_INET,SOCK_STREAM,0);
	if(s_id == -1) {
		perror("Client. Could not Assign Socket");
	}

	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port = htons (PROXY_SERVER_PORT_NO);
	serv_addr.sin_addr.s_addr = inet_addr ("127.0.0.1");

	// requesting to connect using the accept function
	int connect_id=connect(s_id,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	if(connect_id == -1) {
		error("Client. Could not Connect ");
	}

}


void parse_command_line_arguments(int argc, char* argv[], int *totalThreads, int *requestsPerThread ) 
{

    //Parsing the command line arguments
   if( argc == 3 ) {
     	*totalThreads = atoi(argv[1]);
     	*requestsPerThread = atoi(argv[2]);
   }
   else {
       fprintf(stderr, "Usage : %s <Total-threads> <Requests-per-thread>\n", argv[0]);
       exit(1);
   }
}


