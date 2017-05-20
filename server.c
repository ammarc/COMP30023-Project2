/* A simple server in the internet domain using TCP
The port number is passed as an argument 


 To compile: gcc server.c -o server 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "server.h"


int main(int argc, char **argv)
{
	int sockfd, newsockfd, portno, clilen;
	char buffer[256];
	struct sockaddr_in serv_addr;//, cli_addr;
	int n, i;
	int client_sockets[10], max_sd, activity;
	fd_set readfds;

	for (i = 0; i < 10; i++)
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

	
	bzero((char *) &serv_addr, sizeof(serv_addr));

	portno = atoi(argv[1]);
	
	/* Create address we're going to listen on (given port number)
	 - converted to network byte order & any IP address for 
	 this machine */
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);  // store in machine-neutral format

	 /* Bind address to the socket */
	
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr)) < 0) 
	{
		perror("ERROR on binding");
		exit(1);
	}

	//try to specify maximum of 3 pending connections for the master socket 
    if (listen(sockfd, 10) < 0)  
    {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    }  
	
	//clilen = sizeof(cli_addr);
	clilen = sizeof(serv_addr);

	//TODO: change this to accomodate for windows carriage return
	//TODO: not sure about this loop to keep the server running
	while (1)
	{
		// Setting all the file descriptors to 0
		FD_ZERO(&readfds);

		// Adding our original socket to the set
		FD_SET(sockfd, &readfds);
		max_sd = sockfd;

		for (i = 0; i < 10; i++)
		{
			if (client_sockets[i] > 0)
				FD_SET(client_sockets[i], &readfds);

			if (client_sockets[i] > max_sd)
				max_sd = client_sockets[i];
		}

		// This waits for any activity in any of the sockets
		fprintf(stderr, "Waiting for an activity\n");
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		fprintf(stderr, "Finished waiting for an activity\n");

		if ((activity < 0) && (errno!=EINTR))  
            printf("select error");

		//If something happened on the master socket , 
        //then its an incoming connection 
        if (FD_ISSET(sockfd, &readfds))  
        {  
			/* Accept a connection - block until a connection is ready to
			be accepted. Get back a new file descriptor to communicate on. */
            if ((newsockfd = accept(sockfd, 
                    (struct sockaddr *)&serv_addr, (socklen_t*)&clilen)) < 0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
            
			bzero(buffer,256);
			/* Read characters from the connection,
				then process */
		
            //add new socket to array of sockets 
            for (i = 0; i < 10; i++)  
            {  
                //if position is empty 
                if( client_sockets[i] == 0 )  
                {  
                    client_sockets[i] = newsockfd ;  
                    printf("Adding to list of sockets as %d\n" , i);  
                    break;  
                }  
            }
        }

		//else its some IO operation on some other socket
        for (i = 0; i < 10; i++)  
        {
            newsockfd = client_sockets[i];  
                
            if (FD_ISSET(newsockfd , &readfds))  
            {  
				bzero(buffer,256);
                //Check if it was for closing , and also read the 
                //incoming message 
				//TODO: remove magic number for the size of val read
                if ((n = read(newsockfd, buffer, 255)) == 0)  
                {  
                    //Close the socket and mark as 0 in list for reuse 
					fprintf(stderr, "Closing socket\n");
                    close(newsockfd);
                    client_sockets[i] = 0;  
                }
				fprintf(stderr, "Finding ping match with buffer %s\n", buffer);
				if (strncmp(buffer, PING, 4) == 0)
				{
					fprintf(stderr, "Found ping match\n");
					if(send(newsockfd, PONG, strlen(PONG), 0) != strlen(PONG) )  
					{
						perror("ERROR writing to socket");
						exit(1);
					}
				}
				else if (strncmp(buffer, ERRO, 4) == 0)
				{
					BYTE error[40] = {" Can't send ERRO to server\r\n"};
					send_erro(error, newsockfd);
				}
				else if (strncmp(buffer, OKAY, 4) == 0)
				{
					BYTE error[40];
					memset(error, 0, 40);
 					strcpy((char *)error, " Can't send OKAY to server\r\n");
					send_erro(error, newsockfd);
				}
				else if (strncmp(buffer, PONG, 4) == 0)
				{
					BYTE error[40] = {" Can't send PONG to server\r\n"};
					send_erro(error, newsockfd);
				}
				else if (strncmp(buffer, SOLN, 4) == 0)
				{
					//TODO: need to check for formatting as well
					uint32_t difficulty;
					BYTE seed[32];
					uint256_init(seed);
					uint64_t solution;
					char temp[64+1];

					strtok(buffer, " ");

 					difficulty = strtol((strtok(NULL, " ")), NULL, 16);
					strcpy(temp, strtok(NULL, " "));
					temp[64] = '\0';
					for(int i = 62; i >= 0; i-=2)
					{
						char str[2];
						strcpy(str, &temp[i]);
						seed[i/2] = strtol(str, NULL, 16) & 0xFF;
						temp[i] = '\0';
					}

					//strcpy(temp, strtok(NULL, " "));
 					solution = strtol((strtok(NULL, " ")), NULL, 16);

					if (check_sol(difficulty, seed, solution))
					{
						if(send(newsockfd, OKAY, strlen(OKAY), 0) != strlen(OKAY))
						{
							perror("ERROR writing to socket");
							exit(1);
						}
					}
					else
					{
						if(send(newsockfd, "WRONG", strlen("WRONG"), 0) != strlen("WRONG"))
						{
							perror("ERROR writing to socket");
							exit(1);
						}
					}
				}
            }  
        }
		/* Listen on socket - means we're ready to accept connections - 
		incoming connection requests will be queued */
		//listen(sockfd,5);


		//printf("Here is the message: %s\n",buffer);
		
	}
	/* close socket */
	close(sockfd);
	return 0; 
}

void send_erro (BYTE error[40], int newsockfd)
{
	// TODO: remove this magic number
	char error_message[40 + sizeof(ERRO) + 1] = "";
	printf("%s", (char *)error);
	memset(error_message, 0,  40 + sizeof(ERRO) + 1);
	//fprintf(stderr, "After memset\n");
	char *concatenated = malloc(sizeof *error + sizeof(ERRO));
	strcat(concatenated, ERRO);
	strcat(concatenated, (char *)error);
	//printf("Two %s", concatenated);
	//char c = -30;
	//printf("%c", c);
	//strcat(concatenated, )
	//strcpy(error_message,;
	//fprintf(stderr, "Good till now\n");
	if (send(newsockfd, concatenated, strlen(concatenated), 0) != 
												(int)strlen(concatenated))
	{
		perror("ERROR writing to socket");
		exit(1);
	}
}

bool check_sol(uint32_t difficulty, BYTE seed[64], uint64_t solution)
{
	// First we need to concatenate the seed and the solution
	int i;
	BYTE hash[64];
	uint256_init(hash);
	BYTE temp[64];
	uint256_init(temp);
	BYTE target[64];
	uint256_init(target);
	//BYTE alpha[64];
	//uint256_init(alpha);
	//BYTE beta[64];
	//uint256_init(beta);
	uint32_t alpha = 0;
	uint32_t beta = 0;
	// Finding the hash to be checked
	fprintf(stderr, "Seed is: ");
	print_uint256(seed);
	fprintf(stderr, "\n");
	fprintf(stderr, "Solution is: %lld\n", solution);

	for (i = 63; i >= 0; i--)
	{
		hash[i] = seed[i] | (solution & 0xFF);
		solution = solution >> 8;
		fprintf(stderr, "Hash %d is %02x\n", i, hash[i]);
	}
	
	// Extracting alpha
	// We need bits 0..7 for alpha from difficulty (MSB)
	fprintf(stderr, "difficulty is %d\n", difficulty);
	beta = ((1 << 24) - 1) & difficulty;
	fprintf(stderr, "Beta is %d\n", beta);
	alpha = ((1 << 8) - 1) & (difficulty >> 24);
	fprintf(stderr, "Alpha is %d\n", alpha);

	uint32_t exponent;
	//uint32_t store_difficulty = difficulty;
	//difficulty = difficulty << 8;
	//fprintf(stderr, "New difficulty : %u\n", difficulty);
    // Extracting beta
	//beta = ((1 << (24)) - 1) & difficulty;
	//fprintf(stderr, "Beta is %d\n", beta);
	
	// Setting temp to 0
	uint256_init(temp);
	BYTE base[32];
	uint256_init(base);
	uint32_t beta_copy = beta;
	base[31] = 2;
	exponent = 8 * (alpha - 3);
	uint256_exp(temp, base, exponent);
	BYTE beta_rep[3];

	for(int i = 31; i >= 0; i--)
	{
		beta_rep[i] = beta_copy & 0xFF;
		beta_copy = beta_copy >> 8;
	}
    uint256_mul(target, beta_rep, temp);
	fprintf(stderr, "Target is: ");
	print_uint256(target);
	fprintf(stderr, "\n");


	fprintf(stderr, "Hash is: ");
	print_uint256(hash);
	fprintf(stderr, "\n");

	sleep(10);

	for(int i = 0; i < 64; i++)
		if(hash[i] < target[i])
			return true;
        // Checking for the case when they're equal
        /*if (i == 31)
			if (target[i] == hash[i])
				return false;*/
    return false;
}