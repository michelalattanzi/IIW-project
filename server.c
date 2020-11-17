#include<sys/wait.h>
#include "program.h"
#include <dirent.h>
#include <sys/stat.h>


/* ***********************************************************************************
 * get_directory_contents procedure *
*************************************************************************************/

char* get_directory_contents(char* directory_path){

    char* directory_listing = NULL;

  	// open directory
  	DIR* path = opendir(directory_path);
  	if(path != NULL){
		directory_listing = (char*) malloc(sizeof(char)*MAX_BYTE_DIR);
      		directory_listing[0] = '\0';

      		// struct of info on files or subdirectories of directory_path
      		struct dirent* underlying_file = NULL;

      		// readdir returns, if successful, a pointer to the structure containing the information and goes to the next item
      		while((underlying_file = readdir(path)) != NULL){
			strcat(directory_listing, underlying_file->d_name);
          	strcat(directory_listing, "\n");
      		}
      		closedir(path);
  	}
	return directory_listing;
}

/* **********************************************************************************
 * srv_ric procedure *
************************************************************************************/

void srv_ric(struct send_file_args *state,struct frame request){

    struct frame reply;

	// preparation of the response Ack
	reply.server_port= state->local_sin.sin_port;
	reply.type=ACK;

	socklen_t remote_sinlen=sizeof(state->remote_sin);

	//opening the file in write mode; in case of error the process ends after sending the client an ack with the error report
	strncpy(state->new_filename,request.parametro,DIM_NOMEFILE);
	state->f_dest = fopen(request.parametro, "wb");
	if (state->f_dest== NULL){
   		printf("\n\t *** Process %d : error opening destination file(%s) - process ended \n\n ",getpid(),state->new_filename);
		reply.esito=1;
		//sendto --> number of sent byte
		if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
			perror("\n *** Transmission failed in send procedure - process ended \n\n");
    		exit(1);
		}
		exit(1);
    }

	//response to client containing child process port
	reply.esito=0;
	if (sendto(state->s, (void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
		perror("\n *** Transmission failed in send procedure - process ended \n\n");
    		exit(1);
	}
	receive(state);
}

/* ***********************************************************************************
 * srv_trasm procedure *
*************************************************************************************/

void srv_trasm(struct send_file_args *state,struct frame request){

    struct frame reply;

	// preparation of the response Ack
	reply.server_port= state->local_sin.sin_port;
	reply.type=ACK;
    strncpy(reply.istruz,request.istruz,5);

	socklen_t remote_sinlen=sizeof(state->remote_sin);

	// management of the received command
	if (strcmp(request.istruz, "get")==0){

		// opening the source file in read mode; in case of error the process ends after sending the client an ack with the error report
	 	strncpy(state->file_name,request.parametro,DIM_NOMEFILE);

		state->f_src = fopen(request.parametro, "rb");
		if (state->f_src== NULL){
   			printf("\n\t *** Process %d : error opening source file (%s) \n ",getpid(),state->file_name);
			reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("\n *** Transmission failed in send procedure - process ended \n\n");
    			exit(1);
			}
			exit(1);
		}
	}
	else{
		strncpy(state->file_name,request.parametro,DIM_NOMEFILE);
		//check if file exists
  		struct stat file_stat;
 		if (stat(request.parametro, &file_stat) != 0){
   			printf("\n *** Directory %s doesn't exist : process ended \n\n", request.parametro);
	  		reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("\n *** Transmission failed in send procedure - process ended \n\n");
  				exit(1);
			}
   			exit(1);
 		}
  		if (!S_ISDIR(file_stat.st_mode)) {
            printf("\n *** %s it's not a directory - process ended \n\n", request.parametro);
			reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("\n *** Transmission failed in send procedure - process ended \n\n");
                exit(1);
			}
            exit(1);
 		}
 		// write directory content in memory
  		state->path = get_directory_contents(request.parametro);

		if(state->path == NULL){
 		  	fprintf(stderr, "\n *** Error reading the contents of  %s : process ended \n\n", request.parametro);
			reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("\n *** Transmission failed in send procedure - process ended \n\n");
    				exit(1);
			}
  			exit(1);
 		}
	}
	// sending response to client with chid port
	reply.esito=0;
	if (sendto(state->s, (void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
		perror("\n *** Transmission failed in send procedure - process ended \n\n");
    		exit(1);
	}

	// waiting for START command
	while(1){
		TMaxS.tv_sec = TMax_sec;
		TMaxS.tv_usec = 0;
		FD_ZERO(&rset);
        FD_SET(state->s, &rset);
       	maxfdp = state->s+1;
		int n=select(maxfdp, &rset, NULL, NULL, &TMaxS);
		if(n==0){
			printf("\n *** Child %d START waiting time exceeded - process ended \n\n", getpid());
			exit(EXIT_FAILURE);
		}
		if (FD_ISSET(state->s, &rset)){
	 		int l=recvfrom(state->s,(void *)&request, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, &remote_sinlen);
			if (l <= 0)
				continue;
			else break;
		}
	}
	if  (request.type!=START){
		printf("\n *** START command not received, can't start transmission - process ended \n\n");
		exit(EXIT_FAILURE);
	}
	transmit(state);
}

/************************************************************************************************
 * main *
************************************************************************************************/

int main(int argc,char *argv[]){

    int l;
    struct sockaddr_in filesrvaddr;
    struct frame request;
    struct send_file_args *state;

	//Inizialize mutex to global variable access
	int rc = pthread_mutex_init(&mtx1,NULL);
	if (rc !=0){
   		 fprintf(stderr," *** Error in mutex initialization phase \n");
   	 	 return EXIT_FAILURE;
	}

    //setting configuration parameters and log file
    l=read_config(argc,argv,&cfg);
    if (l==1){
		fprintf(stderr," *** Error in initialization phase \n");
   		return EXIT_FAILURE;
	}
   	// parameters struct allocation
   	state = (struct send_file_args*) malloc(sizeof(struct send_file_args));

   	state->lar = 0;
   	state->lfs = 0;
   	state->sws = cfg.sws;
  	state->seq_max = cfg.seq_max;
  	state->f_dest=NULL;
   	state->f_src=NULL;
  	state->path=NULL;

    // setting server address
   	memset(&filesrvaddr,0,sizeof(filesrvaddr));

    filesrvaddr.sin_family=AF_INET;
    filesrvaddr.sin_port=htons(cfg.srv_port);
    filesrvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // opening parent socket
    int filelistenfd=socket(AF_INET,SOCK_DGRAM,0);
	if (filelistenfd < 0 ){
		printf("\n *** Failure socket creation : server process ended \n");
		return EXIT_FAILURE;
	}

	if (bind(filelistenfd, (struct sockaddr *)&filesrvaddr,sizeof(filesrvaddr))<0){
		printf(" *** Bind failure \n");
       	return 0;
	}

	// setting client address
	socklen_t remote_sinlen=sizeof(state->remote_sin);
	memset(&state->remote_sin,0,sizeof(state->remote_sin));


	for (;;){
		// management of client message
		printf(" Waiting for client command ... \n");
		recvfrom(filelistenfd,(void *)&request, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, &remote_sinlen);

		if  (request.type!=CMD){
			printf(" *** Parent %u : unknown request type \n",getpid());
			continue;
		}
		if (strcmp(request.istruz, "stop")==0){
			printf(" *** Parent %u : received server termination command \n",getpid());
			break;
		}
        // child process creation
		pid_t pid=fork();
		if(pid == 0){
			state->s=socket(AF_INET,SOCK_DGRAM,0);
			if (state->s < 0){
				printf(" *** Creation of child socket failed - process ended\n");
				exit(EXIT_FAILURE);
			}
			memset(&state->local_sin,0,sizeof(state->local_sin));
   			state->local_sin.sin_family=AF_INET;
   			state->local_sin.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(state->s, (struct sockaddr *)&state->local_sin,sizeof(state->local_sin))<0){
				printf(" *** Bind failure: process ended \n");
				exit(EXIT_FAILURE);
            }

			// child socket address
			socklen_t local_sinlen=sizeof(state->local_sin);
			getsockname(state->s,(struct sockaddr *)&state->local_sin,&local_sinlen);

			strncpy(state->istruz,request.istruz,5);

			// management of client command

			if ((strcmp(request.istruz, "get")==0) || (strcmp(request.istruz, "list")==0))
				srv_trasm(state,request);
			else
				srv_ric(state,request);

			exit(0);
		}

	}

    close(filelistenfd);
	if (fclose(flog)!=0){
		perror(" *** Error closing log file - process ended \n");
     	exit(1);
	}
	return 0;
}
