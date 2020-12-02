#include "program.h"

// struct contenente lo split del comando digitato
 struct cmd_s {
    char istruz[5];                   // list, get, put
    char file_src[DIM_NOMEFILE];      // source filname
    char file_dest[DIM_NOMEFILE];     // destination filename
    char path[DIM_NOMEFILE];          // path directory to transfer

 };

int check_ex(char a[100], char b[100]){

    char newString1[5];
    char newString2[5];

    int l1=strlen(a);
    int l2=strlen(b);

    int i;
    int count1=0;
    int s=0;
    int j;
    int count2=0;
    int r=0;
	
    for(i=0; i<l1 ; i++){
        if(a[i]=='.'){
           for(count1=i+1; count1<l1 ; count1++){
                newString1[s] = a[count1];
                s++;
           }
        }
    }

    for(j=0; j<l2; j++){
        if(b[j]=='.'){
            for(count2=j+1; count2<l2 ; count2++){
                newString2[r] = b[count2];
                r++;
            }
        }
    }

    if (strcmp(newString1,newString2)==0)
        return 0;
    else
        return 1;

}

int read_cmd(struct cmd_s *cmd){

    char str1[100];
    char newString[3][DIM_NOMEFILE];

    int i,j,ctr;

 	while(1){
        strncpy(cmd->file_src,"",DIM_NOMEFILE);
    	strncpy(cmd->file_dest,"",DIM_NOMEFILE);
        strncpy(cmd->path,"",DIM_NOMEFILE);

   		printf("Enter command: >> ");
   		fgets(str1, sizeof str1, stdin);
   		j=0; ctr=0;

		// input string handling
		for(i=0;i<=(strlen(str1));i++){
            if(str1[i]==' '||str1[i]=='\0' || str1[i]=='\n' ){
                newString[ctr][j]='\0';
                ctr++;
                j=0;
            }
            else{
            	newString[ctr][j]=str1[i];
                j++;
            }
        }
    	strncpy(cmd->istruz,newString[0],5);
		// instruction managment
    	if (strcmp(cmd->istruz, "quit")==0)
           	return(1);
   	if (strcmp(cmd->istruz, "stop")==0)
   	    	return(0);
   	if (strcmp(cmd->istruz, "list")==0) {
       		strncpy(cmd->path,newString[1],DIM_NOMEFILE);
        if ((strcmp(cmd->path, ""))==0){
            printf(" *** Incomplete command: enter the path for the current dir \n\n");
            continue;
            }
            else return(0);
        }
    	else if ((strcmp(cmd->istruz, "get")==0) || (strcmp(cmd->istruz, "put")==0)){
            strncpy(cmd->file_src,newString[1],DIM_NOMEFILE);
            strncpy(cmd->file_dest,newString[2],DIM_NOMEFILE);

		//file extension check
        if ((strcmp(cmd->file_src, ""))!=0 && (strcmp(cmd->file_dest, ""))!=0){
		if(check_ex(cmd->file_src,cmd->file_dest)==1){
		   printf("Check extension");                
		   printf(" *** Source and destination file have not the same extension \n\n");
                   continue;
            }
        }

            if ((strcmp(cmd->file_src, ""))==0 || (strcmp(cmd->file_dest, ""))==0){
                printf(" *** Incomplete command: enter both of filename (src e dst) separated by a single space \n\n");
                continue;

            }
            else return(0);
        }
        else{
            printf(" *** Unknown command, possible choices [put, get, list, quit, stop] \n\n");
            continue;
        }
	}
	return(0);
}

/* **********************************************************************************************
 * main reads configuration file and sets parameters *
 * while loop, reads command line instruction (if 'quit' comes out) *
 * fork: parent returns waiting for a command typed by stdin.
 * child process: fills the fields of the state structure (based on the command received)
   creates the socket and extracts the address sends instruction to the server and waits
   for a reply (if 'trm' the child terminates) receives ack (contrary, the child terminates)
   if instruction is get or list send START command to server and launch procedure receive
   if put instruction launch procedure transmit	*
*************************************************************************************************/

int main(int argc,char *argv[]){

    struct sockaddr_in srvaddr;
    struct frame msg;
    struct frame reply;
    struct cmd_s com;
    struct send_file_args *state;
    int l,fine;

	//Inizialize mutex to global variable access
   	int rc = pthread_mutex_init(&mtx1,NULL);
    if (rc !=0){
       	fprintf(stderr," *** Error in mutex initialization phase \n");
       	return EXIT_FAILURE;
    }
    rc = pthread_mutex_init(&mtx2,NULL);
    if (rc !=0){
       	fprintf(stderr," *** Error in mutex initialization phase \n");
       	return EXIT_FAILURE;
    }

  	// configuration parameters and log file
   	l=read_config(argc,argv,&cfg);
   	if (l==1){
        fprintf(stderr," *** Error in initialization phase \n");
     	return EXIT_FAILURE;
    }
   	// Allocates the send_file_args structure to pass state of communication in the process
   	state = (struct send_file_args*) malloc(sizeof(struct send_file_args));

   	state->lar=0;
   	state->lfs=0;
   	state->seq_max=cfg.seq_max;
   	state->sws=cfg.sws;
   	state->f_dest=NULL;
   	state->f_src=NULL;
   	state->path=NULL;

  	// set server address
   	memset(&srvaddr,0,sizeof(srvaddr));
   	srvaddr.sin_family=AF_INET;
   	srvaddr.sin_port=htons(cfg.srv_port);
   	srvaddr.sin_addr.s_addr = inet_addr(cfg.srv_ip);

   	printf("\nIP remote server %s \n\n",inet_ntoa(srvaddr.sin_addr));
    printf("User Manual: \n put <source filename> <destination filename> \n get <source filename> <destination filename> \n list <path>\n");
    printf(" quit: end of client \n stop: end of server \n\n");

   	while (1){

       	fine=read_cmd(&com);
       	if (fine!=0) {
      		break;
		}

     	// preparing frame to server transmission
        msg.type = CMD;
        strncpy(msg.istruz,com.istruz,5);

        if (strcmp(msg.istruz, "put")==0)
            bcopy(com.file_dest, msg.parametro, strlen(com.file_dest) + 1);
        else if (strcmp(msg.istruz, "get")==0)
            bcopy(com.file_src, msg.parametro, strlen(com.file_src) + 1);
        else if (strcmp(msg.istruz, "list")==0)
            bcopy(com.path, msg.parametro, strlen(com.path) + 1);

        pid_t pid=fork();
        if(pid == 0){
            //  new process to fork for each command
            strncpy(state->istruz,com.istruz,5);

			//file handling in source folder or creation in destination folder
            if (strcmp(com.istruz, "get")==0){
				strncpy(state->file_name,com.file_src,DIM_NOMEFILE);
				strncpy(state->new_filename,com.file_dest,DIM_NOMEFILE);

                state->f_dest = fopen(com.file_dest, "wb"); //write binary mode
                if (state->f_dest== NULL){
                    fprintf(stderr, " *** Process %d - Error opening destination file (%s) - process ended \n",getpid(),state->new_filename);
                    exit(1);
                }
            }
            else if (strcmp(com.istruz, "put")==0){
				strncpy(state->file_name,com.file_src,DIM_NOMEFILE);
				strncpy(state->new_filename,com.file_dest,DIM_NOMEFILE);

                state->f_src = fopen(com.file_src, "rb");
                if (state->f_src== NULL){
                    fprintf(stderr,  " *** Process %d - Error opening source file (%s) - process ended \n",getpid(),state->file_name);
                    exit(1);
                }
            }
            else if  (strcmp(com.istruz, "list")==0){
				strncpy(state->file_name,com.path,DIM_NOMEFILE);
				strncpy(state->new_filename,com.path,DIM_NOMEFILE);

                // malloc for directory content
                state->path = (char*) malloc(sizeof(char)*MAX_BYTE_DIR);
                if (state->path== NULL){
                    fprintf(stderr,  " *** Error memory allocation - process ended \n");
                    exit(1);
                }
            }
            else if  (strcmp(com.istruz, "stop")==0)
                printf("--> Command to stop the server  \n ");
                else{
                    fprintf(stderr,  " *** Unmanaged command - process ended \n");
                    exit(1);
                }
            //start socket for a connectionless protocol
            state->s=socket(AF_INET,SOCK_DGRAM,0); // int socket(int domain,int type,int protocol)
            if (state->s < 0){
                printf(" *** Failure of socket creation - process ended \n");
                exit(EXIT_FAILURE);
            }
            // extracting the client address assigned by the kernel
            socklen_t local_sinlen=(sizeof(state->local_sin)); //#include <sys/socket.h>
            getsockname(state->s,(struct sockaddr *)&state->local_sin,&local_sinlen);

            // bind to associate socket and local port/address
            if (bind(state->s, (struct sockaddr *)&state->local_sin, sizeof(state->local_sin))<0){
                printf("\n *** Bind failure - process ended \n");
                exit(EXIT_FAILURE);
            }

            // send frame to server
            socklen_t srvaddrlen=sizeof(srvaddr);

            if (sendto(state->s, (void *)&msg, sizeof(struct frame), 0, (struct sockaddr *)&srvaddr, srvaddrlen)<0){
                perror("\n *** Transmission failure - process ended \n");
                exit(1);
            }

            // if the client has sent a stop command to the server, the child process stops
            if  (strcmp(com.istruz, "stop")==0){
                close(state->s);
                exit(0);
            }

            // managment of  of the response expected by the server
            while(1) {

                TMaxS.tv_sec = TMax_sec; //10 seconds
                TMaxS.tv_usec = 0;
                FD_ZERO(&rset);
                FD_SET(state->s, &rset);
                maxfdp = state->s+1;
                int n=select(maxfdp, &rset, NULL, NULL, &TMaxS);
                if(n==0){
                    printf("\n\t *** Process %d : the server is not responding - process ended \n", getpid());
                    exit(EXIT_FAILURE);
                }

                // checks that the exit from the select is consequent to the activation of the socket
                if (FD_ISSET(state->s, &rset)){
                    int l=recvfrom(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&srvaddr, &srvaddrlen);
                    if (l <= 0)
                        continue;
                    else break;  // a message has arrived
                }
            }

            // check the outcome of client request
            if (reply.type!=ACK){
                printf("\n\t *** Process %d  error: expected ACK message - process ended\n",getpid());
                exit(EXIT_FAILURE);
            }

            if (reply.esito!=0){
				// server encountered problems; stop child process to prevent zombies from forming
				printf("\n\t *** Process %d  error : server encountered problems in command elaboration - process ended\n",getpid());
                exit(EXIT_FAILURE);
            }

            //server address obtained thanks to the ACK message received
            memset(&state->remote_sin,0,sizeof(state->remote_sin));
            state->remote_sin.sin_family=AF_INET;
            state->remote_sin.sin_port=reply.server_port;
            state->remote_sin.sin_addr.s_addr = inet_addr(cfg.srv_ip);

            socklen_t remote_sinlen=sizeof(state->remote_sin);

            // managment of client receiving
            if ((strcmp(com.istruz, "get")==0) || (strcmp(com.istruz, "list")==0)){
                // start command to server
                msg.type=START;

                if (sendto(state->s, (void *)&msg, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
					perror("\n *** Transmission failure - process ended \n");
                    exit(1);
                }
                receive(state); // receiving procedure on client
            }
            else if (strcmp(com.istruz, "put")==0){
				transmit(state); // transmission procedure on client
            }
			close(state->s);
                	exit(0);
        }
    }

    if (fclose(flog)!=0){
		perror(" *** Error closing log file - process ended  \n");
        exit(1);
    }
    printf(" End of client\n");
    exit(0);
}
