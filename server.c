/*
 *  Project 2 for COMP30023: Computer Systems
 *  at the University of Melbourne
 *  Semester 1, 2017
 *  by: Ammar Ahmed
 *  Username: ammara
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>
#include "server.h"
#include "mine.h"
#include "list.h"
#include "input-handler.h"

FILE* log_file;
List* work_queue;

int main(int argc, char **argv)
{
	int sockfd, newsockfd, portno, servlen;
	struct sockaddr_in address;
	pthread_t tid;
	int n, i, j, k = 0;
	int client_sockets[MAX_CLIENTS], max_sd, activity;
	fd_set readfds;
	work_queue = new_list();
    pthread_create(&tid, NULL, (void*)&search_for_work, NULL);
	// a buffer for each of the clients
	char** buffers = malloc(sizeof(char*)*100);
	char temp[256];
	memset(temp, '\0', 256);
	log_file = fopen("log.txt", "w");
	// Printing descriptors in file heading
	fprintf(log_file, "Time    IP    SocketID    Message\n");
	fflush(log_file);

	// Handling broken pipes
	signal(SIGPIPE, SIG_IGN);

	for (i = 0; i < MAX_CLIENTS; i++)
		client_sockets[i] = 0;

	if (argc < 2) 
	{
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}

	 /* Create TCP socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) 
	{
		perror("ERROR opening socket");
		exit(1);
	}

	
	bzero((char *) &address, sizeof(address));

	portno = atoi(argv[1]);
	
	/* Create address we're going to listen on (given port number)
	 - converted to network byte order & any IP address for 
	 this machine */
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(portno);  // store in machine-neutral format

	 /* Bind address to the socket */
	
	if (bind(sockfd, (struct sockaddr *) &address,
			sizeof(address)) < 0) 
	{
		perror("ERROR on binding");
		exit(1);
	}

	//try to specify maximum of 100 pending connections for the master socket 
    if (listen(sockfd, MAX_CLIENTS) < 0)
    {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    }  
	
	servlen = sizeof(address);

	//TODO: change this to accomodate for windows carriage return
	while (true)
	{
		// Setting all the file descriptors to 0
		FD_ZERO(&readfds);

		// Adding our original socket to the set
		FD_SET(sockfd, &readfds);
		max_sd = sockfd;

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (client_sockets[i] > 0)
				FD_SET(client_sockets[i], &readfds);

			if (client_sockets[i] > max_sd)
				max_sd = client_sockets[i];
		}

		// This waits for any activity in any of the sockets
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno!=EINTR))  
            		printf("select error");

		//If something happened on the master socket , 
        //then its an incoming connection 
        if (FD_ISSET(sockfd, &readfds))  
        {  
			/* Accept a connection - block until a connection is ready to
			be accepted. Get back a new file descriptor to communicate on. */
            if ((newsockfd = accept(sockfd, 
                    (struct sockaddr *)&address, (socklen_t*)&servlen)) < 0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
            
			/* Read characters from the connection,
				then process */
		
            //add new socket to array of sockets 
            for (i = 0; i < MAX_CLIENTS; i++)  
            {  
                //if position is empty 
                if( client_sockets[i] == 0 )  
                {  
                    client_sockets[i] = newsockfd ;  
					buffers[i] = malloc(sizeof(char)*256);
					memset(buffers[i], '\0', 256);
                    //printf("Adding to list of sockets as %d\n" , i);  
                    break;  
                }  
            }
        }

		//else its some IO operation on some other socket
		//TODO: change this to handle clients
        for (i = 0; i < MAX_CLIENTS; i++)  
        {
            newsockfd = client_sockets[i];
            if (FD_ISSET(newsockfd , &readfds))  
            {  
				//fprintf(stderr, "newsockfd is %d\n", newsockfd);
                //Check if it was for closing , and also read the 
                //incoming message 
				//TODO: remove magic number for the size of val read
				n = read(newsockfd, temp, 256);
				log_to_file(temp, inet_ntoa(address.sin_addr), newsockfd);
				if (n+strlen(buffers[i]) > 256)
				{
					send_erro((BYTE*)"Message length too long", newsockfd);
					continue;
				}
				//fprintf(stdout, "Temp is %s\n", temp);
				//fprintf(stdout, "Buffer is: %s\n", buffers[i]);
				//fflush(stdout); 
				strcat(buffers[i], temp);
				memset(temp, '\0', 256);
                if (n == 0)
                {  
                    // Close the socket and mark as 0 in list for reuse
					// We also need to free all its jobs in the queue
					handle_abrt(newsockfd);
					free(buffers[i]);
                    close(newsockfd);
                    client_sockets[i] = 0;  
                }
				else
				{
					/*int j = 0;
					while(buffers[i][j] != '\0')
					{
						char *first_msg = malloc(sizeof(char) * (256 + 1)); 
						char *rem_msg= malloc(sizeof(char) * (256 + 1));
						memset(rem_msg, '\0', 256 + 1); 
						if(j != 256 - 1 && buffers[i][j] == '\r' && 
								buffers[i][j + 1] == '\n')
						{
							strncpy(first_msg , buffers[i], j);
							first_msg [j] = '\0';
							// Adding 3 for null byte and carriage return
							strcpy(rem_msg, buffers[i] + j + 3);
							handle_input(newsockfd, first_msg , work_queue);
							memset(buffers[i], '\0', 256 + 1);
							strcpy(buffers[i], rem_msg); 
							free(rem_msg);
							free(first_msg);
							j += 2;    
						}
						else
							j++;
					}*/
					
					for (j = 0; j < 256; j++, k++)
					{
						if (buffers[i][j] == '\0')
							break;

						if (j != 255 && buffers[i][j] == '\r' && buffers[i][j+1] == '\n')
						{
							//fprintf(stdout, "j is: %d\n", j); 
							//fflush(stdout); 
							char *message_to_process = malloc(sizeof(char) * 257);
							strncpy(message_to_process, buffers[i], j);
							message_to_process[j] = '\0'; 
							handle_input(newsockfd, message_to_process, work_queue);
							k = 0;
							j += 2;
							memset(buffers[i], 0, 256);
							memset(message_to_process, '\0', 257);
							continue;
						}
					}
				}
            }
        }
	}
	/* close socket */
	close(sockfd);
	return 0; 
}

void log_to_file(char* msg, char* ip, int sockfd)
{
	struct tm* loc_time;
	time_t curr_time;

	curr_time = time(NULL);
	loc_time = localtime(&curr_time);

	fprintf(log_file, "%02d:%02d:%02d, ", loc_time->tm_hour,
									loc_time->tm_min, loc_time->tm_sec);

	// Now we need to put in the IP-Address
	fprintf(log_file, "%s, ", ip);

	fprintf(log_file, "%d, ", sockfd);

	fprintf(log_file, "%s\n\n", msg);
	fflush(log_file);
}


void search_for_work()
{
	fprintf(stdout, "IN\n"); fflush(stdout);
	while(true)
	{
		if (work_queue->size > 0)
		{
			struct work_args * args = (struct work_args*)work_queue->head->data;
			work(*args);
			list_remove_start(work_queue);
		}
	}
}