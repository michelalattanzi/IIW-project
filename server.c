#include<sys/wait.h>
#include "uftp.h"
#include <dirent.h>
#include <sys/stat.h>


/* ***********************************************************************************
 * la procedura get_directory_contents						     *
 *  - Estrae il contenuto della directory indicata da directory_path e lo riporta    *
 *  - in directory_listing   							     *
 *************************************************************************************/

char* get_directory_contents(char* directory_path)
{
char* directory_listing = NULL;
  
  	// apre la directory 
  	DIR* path = opendir(directory_path);

  	// Controllo successo apertura della directory
  	if(path != NULL){
		directory_listing = (char*) malloc(sizeof(char)*MAX_BYTE_DIR);
      		directory_listing[0] = '\0';

      		//Struct nella quale vengono immagazzinate info sui files o sotto-directory di directory_path	
      		struct dirent* underlying_file = NULL;

      		/* Readdir ritorna,in caso di successo, un puntatore alla struttura contenente le informazioni
	 	e si posiziona sulla voce successiva, è possibile quindi iterare in modo da leggere l'intero contenuto*/
  
      		while((underlying_file = readdir(path)) != NULL){
			strcat(directory_listing, underlying_file->d_name);
          	strcat(directory_listing, "\n");
      		}
      		closedir(path);
  	}
	return directory_listing;
}

/* **********************************************************************************
 * la procedura srv_ric								    *
 *  - Prepara ack di risposta    						    *
 *  - Apre file di destinazione in scrittura (ack con esito negativo e arresto del  *
 *    processo in caso di errore)						    *
 *  - Invia al client ack con esito positivo ed indirizzo del processo figlio       *
 *  - Lancia procedura ricevi 							    *
 ************************************************************************************/

void srv_ric(struct send_file_args *state,struct frame request)

{
struct frame reply;


	/* -------------------------------------------*		
	 *    preparazione Ack di risposta            *                  
	 * -------------------------------------------*/

	reply.server_port= state->local_sin.sin_port;  
	reply.type=ACK; 
        
	socklen_t remote_sinlen=sizeof(state->remote_sin);

	/* --------------------------------------------------------------------*	
	 * apre il file in scrittura ed in caso di errore termina il processo  *
         * dopo aver inviato al client un ack con la segnalazione di errore    *               
	 * --------------------------------------------------------------------*/
	strncpy(state->new_filename,request.parametro,DIM_NOMEFILE);	 
	state->f_dest = fopen(request.parametro, "wb");		                           
	if (state->f_dest== NULL){
   		printf("\tProcesso %d : ********* Errore apertura file destinazione(%s), esco dal processo  \n ",getpid(),state->new_filename);
		reply.esito=1;
		if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
			perror("Trasmissione fallita in procedura send : esco dal processo   \n");
    			exit(1);
		}
		exit(1);
      	}
				
	/* -----------------------------------------------------------*	
	 * risposta al client contenente la porta del processo figlio * 
	 * -----------------------------------------------------------*/

	reply.esito=0;
	if (sendto(state->s, (void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
		perror("Trasmissione fallita in procedura send : esco dal processo   \n");
    		exit(1);
	}
	
	ricevi(state);

}

/* ***********************************************************************************
 * la procedura srv_trasm							     *
 *  - Prepara ack di risposta      						     *
 *  - Se istruzione get apre il file origine in lettura, se list scrive contenuto    *
 *    directory in area di memoria (in entrambi in casi, ack con esito negativo e    *
 *    terminazione processo in caso di errore)					     *
 *  - Invia al client ack positivo ed indirizzo del processo figlio 		     *
 *  - Attende comando START, appena ricevuto richiama procedura trasmetti.	     *
 *************************************************************************************/

void srv_trasm(struct send_file_args *state,struct frame request)

{
struct frame reply;

	/* --------------------------------------*		
	 *   preparazione Ack di risposta        *                     
	 * --------------------------------------*/

	reply.server_port= state->local_sin.sin_port;  
	reply.type=ACK; 
    	strncpy(reply.istruz,request.istruz,5);
    
	socklen_t remote_sinlen=sizeof(state->remote_sin);

	/* --------------------------------------------*		
	 *  gestione del comando ricevuto  dal client  *                
	 * --------------------------------------------*/
			
	if (strcmp(request.istruz, "get")==0){	
		
		/* ----------------------------------------------------------------------------*	
	 	 * apre il file di origine in lettura ed in caso di errore termina il processo *
		 * dopo aver inviato al client un ack con la segnalazione di errore            *               
		 * ----------------------------------------------------------------------------*/		
	 
		strncpy(state->file_name,request.parametro,DIM_NOMEFILE); 
							
		state->f_src = fopen(request.parametro, "rb");	 			                            
		if (state->f_src== NULL){
   			printf("\tProcesso %d : ********* Errore apertura file sorgente (%s)  \n ",getpid(),state->file_name);
			reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("Trasmissione fallita in procedura send : esco dal processo   \n");
    				exit(1);
			}
			exit(1);
		}     	 		
	}
	else{ 	
	
		/* ----------------------------------------------------------------------------*	
	 	 * Verifica se esiste una directory corrispondente al path specificato,in caso *
		 * di errore,dopo aver inviato al client un ack con la segnalazione di errore, *
                 * termina il processo.            					       *
		 * ----------------------------------------------------------------------------*/
		
		strncpy(state->file_name,request.parametro,DIM_NOMEFILE); 
		//verifica se il file esiste
  		struct stat file_stat;
 		if (stat(request.parametro, &file_stat) != 0){
   			printf("La directory %s non esiste , termino il processo  \n", request.parametro);
	  		reply.esito=1;	
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("Trasmissione fallita in procedura send : esco dal processo   \n");
  				exit(1);
			}
   			exit(1);
 		}

		//verifica se si tratta di una directory
  		if (!S_ISDIR(file_stat.st_mode)) {
    			printf("%s non è una directory, termino il processo \n", request.parametro);
			reply.esito=1;	
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("Trasmissione fallita in procedura send : processo  arrestato  \n");
    				exit(1);
			}
    			 exit(1);
 		}

 		// scrive in memoria il contenuto della directory
  		state->path = get_directory_contents(request.parametro); 
 		  	
		if(state->path == NULL){
 		  	fprintf(stderr, "Errore leggendo il contenuto di  %s processo arrestato \n", request.parametro);
			reply.esito=1;
			if (sendto(state->s,(void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
				perror("Trasmissione fallita in procedura send : processo arrestato   \n");
    				exit(1);
			}	
  			exit(1);
 		}
	}
		
					
	/* -----------------------------------------------------*		
	 * invia risposta al client con esito positivo e	*
         * contenente la porta del processo figlio  		*
	 * -----------------------------------------------------*/

	reply.esito=0;
	if (sendto(state->s, (void *)&reply, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, remote_sinlen)<0){
		perror("Trasmissione fallita in procedura send : esco dal processo   \n");
    		exit(1);
	}
	
	/* -----------------------------------------------------*		
	 * Attesa del comando start e arresto del processo	*
 	 * qualora non arrivi entro il tempo massimo impostato	*
	 * -----------------------------------------------------*/
		
	while(1){									
		TMaxS.tv_sec = TMax_sec;
		TMaxS.tv_usec = 0;
		FD_ZERO(&rset);
        	FD_SET(state->s, &rset);
       		maxfdp = state->s+1;
		int n=select(maxfdp, &rset, NULL, NULL, &TMaxS);
		if(n==0){
			printf("     figlio %d : Tempo massimo attesa START superato, termino il processo \n", getpid());
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
		printf("non ricevuto comando START Non posso iniziare la trasmissione Esco dal processo \n");
		exit(EXIT_FAILURE);
	}
	
      	        				
	//inizia la trasmissione 				
	trasmetti(state);
}

/************************************************************************************************
 * Main:											*
 *  - Legge file di configurazione ed imposta parametri    					*
 *  - Imposta indirizzo server e crea socket							*
 *  - Ciclo while: attende un messaggio da client (se msg non è di tipo CMD ignora e torna	*
 *    in attesa), se istruzione trm il processo termina. Altrimenti crea un processo figlio	*
 *    e torna in attesa di nuovi messaggi.							*
 *    il figlio:										*
 *  - Imposta indirizzo del processo figlio e crea una socket					*
 *  - Se istruzione get o list chiama la procedura srv_trasm, se put chiama srv_ric.		*  
 ************************************************************************************************/

int main(int argc,char *argv[])
{
int l;
struct sockaddr_in filesrvaddr;
struct frame request;  	 
struct send_file_args *state;

	//Inizializzazione mutex per accesso a variabili globali
	int rc = pthread_mutex_init(&mtx1,NULL);
	if (rc !=0){
   		 fprintf(stderr,"********* Errore in fase di inizializzazione mutex \n");
   	 	 return EXIT_FAILURE;
	} 
	rc = pthread_mutex_init(&mtx1,NULL);
	if (rc !=0){
		fprintf(stderr,"********* Errore in fase di inizializzazione mutex \n");
   	 	 return EXIT_FAILURE;
	} 
	


	/* ----------------------------------------------------------------*
	 * impostazione dei parametri di configurazione e del file di log  *
 	 * ----------------------------------------------------------------*/
   
     	l=leggi_conf(argc,argv,&cfg);
     	if (l==1){
		fprintf(stderr,"********* Errore in fase di inizializzazione ,\n");
   		return EXIT_FAILURE;
	} 

     	// Alloca la struct per i parametri
     	state = (struct send_file_args*) malloc(sizeof(struct send_file_args));

     	// Inizializzazione degli altri parametri
    	state->lar = 0;
     	state->lfs = 0;
     	state->sws = cfg.sws;
    	state->seq_max = cfg.seq_max;
    	state->f_dest=NULL;
     	state->f_src=NULL;
    	state->path=NULL;
							

	/* -----------------------------------*
	 * impostazione indirizzo del server  *
	 * -----------------------------------*/
	
   	memset(&filesrvaddr,0,sizeof(filesrvaddr));
	
    	filesrvaddr.sin_family=AF_INET;
     	filesrvaddr.sin_port=htons(cfg.srv_port);
     	filesrvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    
    	 /* --------------------------------------*
	  * apertura  della socket processo padre *          
          *---------------------------------------*/

    	int filelistenfd=socket(AF_INET,SOCK_DGRAM,0);
	if (filelistenfd < 0 ){
		printf("creazione della socket fallita - processo server terminato \n");
		return EXIT_FAILURE;
	}
	
	if (bind(filelistenfd, (struct sockaddr *)&filesrvaddr,sizeof(filesrvaddr))<0){
		printf("fallita Bind\n");
        	return 0;
	}

	/* ---------------------------------------------*
	 * impostazione struttura indirizzo del client  *
	 * ---------------------------------------------*/

	socklen_t remote_sinlen=sizeof(state->remote_sin);
	memset(&state->remote_sin,0,sizeof(state->remote_sin));
	

	for (;;){
		/*  ------------------------------------*
		 *  Attesa e gestione msg dal client    *
		 *  ------------------------------------*/
	
		printf("In attesa di ricevere un comando dal client :\n");
		recvfrom(filelistenfd,(void *)&request, sizeof(struct frame), 0, (struct sockaddr *)&state->remote_sin, &remote_sinlen);
		//gestire uscita in caso di errore nell'istruzione		
		if  (request.type!=CMD){	
			printf("padre %u : tipo richiesta sconosciuto\n",getpid());
			continue;	
		}
		//arresto del processo server in caso di comando 'trm'
		if (strcmp(request.istruz, "trm")==0){
			printf("padre %u : ricevuto comando di arresto del server \n",getpid());
			break;	
		}

		/* ----------------------------*
		 * creazione processo figlio   *
		 * ----------------------------*/
		
		pid_t pid=fork();
		if(pid == 0){		
			/* ------------------------------------------*
			 * Apertura socket processo figlio e Bind    *
			 * ------------------------------------------*/

			state->s=socket(AF_INET,SOCK_DGRAM,0);
			if (state->s < 0){
				printf("creazione della socket processo figlio fallita, esco dal processo  \n");
				exit(EXIT_FAILURE);
			}
 

			memset(&state->local_sin,0,sizeof(state->local_sin));
     			state->local_sin.sin_family=AF_INET;
    			state->local_sin.sin_addr.s_addr = htonl(INADDR_ANY);			
		
			if (bind(state->s, (struct sockaddr *)&state->local_sin,sizeof(state->local_sin))<0){
				printf("bind processo figlio  fallita, esco dal processo \n");
				exit(EXIT_FAILURE);	
        		}
							
			// estrazione indirizzo da associare alla socket processo figlio
								
			socklen_t local_sinlen=sizeof(state->local_sin);
			getsockname(state->s,(struct sockaddr *)&state->local_sin,&local_sinlen);		
										
			// completa informazioni  sulla struct state 
			strncpy(state->istruz,request.istruz,5);

			/* -------------------------------------------*		
			 * gestione del comando ricevuto dal client   *
			 * -------------------------------------------*/
					
			if ((strcmp(request.istruz, "get")==0) || (strcmp(request.istruz, "list")==0))					
				srv_trasm(state,request);    //Richiama procedura di trasmissione
			else
				srv_ric(state,request); // richiama procedura di ricezione 

			// esci dal processo figlio 
			exit(0);
		}			
  		/*else{
			int status;
			waitpid(pid,&status,WNOHANG);
		}*/

	}  // end for

    	close(filelistenfd);
	if (fclose(flog)!=0){
		perror("Errore nella chiusura del file di log, esco dal processo \n");
     		exit(1);
	}
	return 0;
}
