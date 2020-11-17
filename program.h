#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <math.h>
#include<arpa/inet.h>
#include<sys/time.h>
#include<errno.h>
#include<limits.h>

#define	DIM_NOMEFILE 100        // max lenght of filename or directory
#define	MAX_BYTE_DIR 2056       // size of memory area allocated to directory contents
#define DATA_SIZE 512		    // standard frame size
#define BUF_SIZE 64    		    // buffer size for client sliding window
#define	COEF_MAX 64 		    // maximum RTO multiplication value
#define	max_line 132     	    // file configuration line size

// frame flags
typedef enum{EMPTY, DATA, ACK, END,CMD,START} frametype_t;  // frame type
typedef uint32_t seqnum_t;                          		// sequence number (unsigned int 32 bit)

// general frame structure
struct frame{
  frametype_t type; 			    // CMD, DATA,START,......
  char istruz[5];        		    // list,get,put
  char parametro[DIM_NOMEFILE];		// name of file or directory to transfer
  int esito;                   		// request result 0 = ok
  seqnum_t seq;                		// frame sequence number
  int eof_pos;                 		// position in the send buffer: -1, if != -1 end of file
  unsigned char data[DATA_SIZE];	// data buffer
  uint32_t time_trasm; 		        // transmission time (milliseconds)
  int ripet ; 				        // 1=file transmitted, 0=no
  uint16_t server_port;   		    // server port
};

// ack message, for frame arrival communication
struct ack{
  frametype_t type;					// ACK or END
  seqnum_t seq;						// sequence number of related frame
  uint32_t time_trasm; 				// transmission time
  int ripet ;          				// 1=retransmission, 0=no
};

// struct for timeout thread parameters
struct timeout_args{
  int s;					        // socket id
  unsigned int duration;			// RTO = retransmission interval
  struct frame *frame; 				// frame to retransmit
  struct sockaddr_in remote_sin;	// local and remote computer addresses
};

// struct of slot in transmission buffer
struct send_slot{
  struct timeout_args timeout_state;// retransmission data
  struct frame send_frame;          // frame to transmit
  pthread_t timeout;                // thread number
  int has_ack; 					    // 1=ack received, 0=no
};

// struct to transfer parameters between different procedures
struct send_file_args{
  struct sockaddr_in remote_sin, local_sin;	// address
  int s; 						            // id socket
  char file_name[DIM_NOMEFILE];				// source filename
  char new_filename[DIM_NOMEFILE];			// destination filename
  FILE *f_src, *f_dest;					    // source and destination files
  char *path; 						        // path of directory (list command)
  char istruz[5];					        // get,put,list
  seqnum_t lar; 					        // last ack received
  seqnum_t lfs; 					        // last frame sent
  seqnum_t sws; 					        // transmission window size
  seqnum_t seq_max;					        // max sequence number
  struct send_slot frame_buf[BUF_SIZE];     // frame buffer
};

// struct of configuration parameters
struct config{
  char srv_ip[50];   // server ip
  int  srv_port;     // port server
  int  tip_RTO;      // RTO type (0=adaptive, 1=fixed)
  float   RTO_in;    // starting value of RTO
  seqnum_t seq_max;	 // max sequence number
  seqnum_t sws;      // windows size
  int rit_send; 	 // transmission interval between two packets
  int rit_ack;		 // minimum delay ack reception
  int max_tent;      // max number of retransmission
  int prob;			 // loss probability
  int t_max;         // maximum seconds of waiting for a package
  int coef_RTO;      // doubles RTO at each retransmission (1=yes, 0=no)
  int coef_RTD;
};

struct config cfg;

// struct for retransmission timeout elaboration with tip_RTO = 0
struct rtt_info {
  float		rtt_rtt;	    // last RTT
  float		rtt_srtt;	    // weighted average SRTT
  float		rtt_rttvar;	    // weighted mean deviance MSDEV
  float		rtt_rto;	    // current value of RTO
  int		rtt_nrexmt;	    // max number of frame retransmission
  uint32_t	rtt_base;	    // transmission start time in milliseconds
};

static struct rtt_info rttinfo; // RTO managment variable

struct timeval	tv; //elapsed time
int rtt_d_flag = 0;	// debug flag; user can change this value
float RTO;			// RTO  frame interval
float RTD;			// delay
FILE *flog;         // log file

// variables used to manage the maximum waiting time for a response
fd_set rset; 			    // set of descriptor for select
int maxfdp;   			    // max number of file descriptor
struct timeval TMax,TMaxS; 	// max waiting time: TMax for config param, TMaxS for starting connection time
int TMax_sec=10;            // waiting time in client-server connection
pthread_mutex_t mtx1,mtx2;	//mutex for accessing variables

// transmission counters to keep track of transmission stats
int tot_frame=0; 		    // transmitted frames
int tot_lost_frame=0; 	    // transmitted lost frames
int tot_frame_rip=0;        // total number of repetitions in frame retransmission
int tot_lost_rep_frame=0;   // retransmitted lost frames
int tot_ack=0;              // ack
int tot_lost_ack=0; 		// lost ack
int tot_frame_fs=0;         // out of sequence frames

// reception counters to keep track of receiving stats
 int ric_tot_frame_pv=0; 	// received frames
 int ric_tot_frame_fs=0;    // out of sequence frames
 int ric_tot_frame=0; 		// total number of written frames

/*************************************************************************************************
* read configuration procedure: returns 0 on success *
*************************************************************************************************/

int read_config(int argc, char *argv[],struct config *cfg){

    char *file_conf,*file_log;
    char linea[max_line];
    char *p;
    FILE *fc;

	printf("\n ********** file configuration parameters ***************** \n");
    if (argc !=3){
        printf(" ***  Enter: %s <name of file configuration> <name of log file> \n", argv[0]);
		return (1);
    }
	//open log file in write mode
	file_log = argv[2];
	flog = fopen(file_log, "w");
	if (flog== NULL){
   	 	fprintf(stderr, " *** Error opening log file \n");
   		return (1);
    }
	// open configuration file in read mode
	file_conf = argv[1];
   	fc = fopen(file_conf, "r");
    if (fc == NULL){
        fprintf(stderr," *** Error opening configuration file \n");
        return (1);
    }
	fprintf(flog,"Configuration file: %s \n",file_conf);
	fprintf(flog,"\nParameters values: \n");

	// set configuration parameters

	p = fgets(linea,max_line,fc);
    strncpy(cfg->srv_ip,linea,15);
    fprintf(flog,"%s",linea);
    printf(" Server IP: %s \n",cfg->srv_ip);

    p = fgets(linea,max_line,fc);
    cfg->srv_port = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Server port: %d \n",cfg->srv_port);

	p = fgets(linea,max_line,fc);
    cfg->prob = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
   	printf(" Loss packet probability: %d \n",cfg->prob);

	p = fgets(linea,max_line,fc);
    cfg->tip_RTO = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" RTO type: %d \n",cfg->tip_RTO);

	p = fgets(linea,max_line,fc);
    cfg->RTO_in = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Starting RTO: %.3f \n",cfg->RTO_in);

	p = fgets(linea,max_line,fc);
    cfg->seq_max = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Max sequence number: %d \n",cfg->seq_max);

	p = fgets(linea,max_line,fc);
    cfg->sws = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Window size: %d \n",cfg->sws);

	p = fgets(linea,max_line,fc);
    cfg->rit_send = strtoul(linea,&p,10);
 	fprintf(flog,"%s",linea);
    printf(" Transmission interval: %d \n",cfg->rit_send);

	p = fgets(linea,max_line,fc);
    cfg->rit_ack = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Waiting time for ack reception: %d \n",cfg->rit_ack);

	p = fgets(linea,max_line,fc);
    cfg->max_tent = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Max number of retransmission:  %d \n",cfg->max_tent);

	p = fgets(linea,max_line,fc);
    cfg->t_max = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Maximum seconds of waiting for a package: %d \n",cfg->t_max);

	p = fgets(linea,max_line,fc);
    cfg->coef_RTO = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Multiplicative coefficient RTO: %d \n",cfg->coef_RTO);

	p = fgets(linea,max_line,fc);
    cfg->coef_RTD = strtoul(linea,&p,10);
	fprintf(flog,"%s",linea);
    printf(" Multiplicative coefficient of delay: %d \n\n",cfg->coef_RTD);

	if (fclose(fc) != 0){
		perror(" *** Error closing configuration file - read_config procedure, I exit the process \n");
     	exit(1);
	}
	printf(" **************************************************************\n\n");
	return (0);
}

/*************************************************************************************************
 * procedure for RTO (retrasmission time out) management *
 ************************************************************************************************/

// RTO calculation formula
#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4.0 * (ptr)->rtt_rttvar))

/*************************************************************************************************
 * procedure to set starting value and manage adaptive timeout *
*************************************************************************************************/

void rtt_init(struct rtt_info *ptr){

	struct timeval	tv;
	gettimeofday(&tv, NULL);
	ptr->rtt_base = (tv.tv_sec * 1000) + (tv.tv_usec / 1000); // milliseconds elapsed since 1/1/1970
	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = cfg.RTO_in;
	ptr->rtt_rttvar = 0.75;
	ptr->rtt_rto=0;
}

/*************************************************************************************************
 * rtt_ts procedure: gets time passed from start *
*************************************************************************************************/

uint32_t rtt_ts(struct rtt_info *ptr){

    struct timeval	tv;
    uint32_t ts;

	gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec  * 1000) + (tv.tv_usec / 1000))-ptr->rtt_base; //time in milliseconds
	return(ts);
}

/*************************************************************************************************
 * rtt_calc procedure: gets RTO value (fixed or adaptive) *
*************************************************************************************************/

float  rtt_calc(struct rtt_info *ptr, uint32_t ms){

    double delta;

	if (cfg.tip_RTO ==1) {
		// RTO_in can't change: if fixed --> value of file configuration
		ptr->rtt_rto=cfg.RTO_in;
		return ptr->rtt_rto;
	}
	// if tip_RTO = 0, calculation with given formula
	ptr->rtt_rtt = ms ;
	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += delta / 8;				/* g = 1/8 */
	if (delta < 0.0)
		delta = -delta;					/* |delta| */
	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	/* h = 1/4 */
	ptr->rtt_rto = RTT_RTOCALC(ptr);
    return ptr->rtt_rto;
}

/*************************************************************************************************
 * rtt_print procedure: optionally write struct camp in log file *
*************************************************************************************************/

void rtt_print(struct rtt_info *ptr){
	if (rtt_d_flag == 0){
		return;
	}
   	fprintf(flog, "rtt = %.3f,      srtt = %.3f,        rttvar = %.3f  \n",
	ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar);
}

/*************************************************************************************************
 * read_mem procedure: read value and write it on frame's data, return 1 --> end of memory area *
 ************************************************************************************************/

int read_mem(char **fm, struct frame* f, seqnum_t seq){

	f->seq = seq;
  	f->type = DATA;
  	f->eof_pos = -1;    //generic value for not ended file

  	strncpy((char *)f->data,*fm,DATA_SIZE);
  	int len = strlen((char *)f->data);
  	if (len < DATA_SIZE){
		f->eof_pos = len; //end of file
        return 1;
    }
  	else
  		*fm+=len;
  	return 0;
}

/*************************************************************************************************
 * write_mem procedure:	read buffer content and write it on memory *
*************************************************************************************************/

int write_mem(char *fm, struct frame *f){

int num_bytes;
int retval;

	if(f->eof_pos < 0){
        num_bytes = DATA_SIZE;
   		retval = 0;
  	}
  	else{
   		num_bytes = f->eof_pos; ; //end of file
   		retval = 1; // last frame
  	}
 	strncat(fm,(char *)f->data,num_bytes);
  	return retval;
}

/*************************************************************************************************
 *  readFrame procedure: read frame fields and write them in data *
*************************************************************************************************/

int read_frame(FILE *fp, struct frame* f, seqnum_t seq){
    int i;
  	f->seq = seq;
  	f->type = DATA;
  	f->eof_pos = -1;

  	fprintf(flog,"       IN READ_FRAME --> Read from file %d \n",seq);

  	for(i = 0; i < DATA_SIZE; i++){
  		f->data[i] = fgetc(fp);
    	if(feof(fp)){
            f->eof_pos = i; //end of file
   	   		return 1;
        }
  	}
  	return 0;
}

/*************************************************************************************************
 *  writeFrame procedure: write received data on destination file *
*************************************************************************************************/

int write_frame(FILE *fp, struct frame *f){
    int i;
    int c;
    int num_bytes;
    int retval;

  	fprintf(flog, "       IN WRITE_FRAME --> Write on file \n");

  	if(f->eof_pos < 0){
        num_bytes = DATA_SIZE;
    	retval = 0;
  	}
  	else{
    	num_bytes = f->eof_pos;
    	retval = 1; //end of file, last frame!
  	}
  	for(i = 0; i < num_bytes; i++){
    	c = f->data[i];
    	fputc(c, fp);
  	}
  	return retval;
}

/*************************************************************************************************
 * seq_check procedure: SELECTIVE REPEAT implementation, checks if frame's sequence number *
 * is within the sliding window (checking on window num of sequence) *
*************************************************************************************************/

int seq_check(seqnum_t win_base, seqnum_t win_size, seqnum_t frame_num, seqnum_t seq_max){

    int retval;

  	seqnum_t win_last = (win_base + win_size - 1) % seq_max;

   	if( win_base > win_last) //number of sequence passed seq_max and restarted
        retval = !(frame_num > win_last && frame_num < win_base);
   	else
       	retval = frame_num <= win_last && frame_num >= win_base;
  	return retval;
}

/**************************************************************************************************
 * thread timeout: thread have to transmit the same frame to a maximum amount of max_tent times *
 **************************************************************************************************/

void* timeout(void *args){

    int i;
    struct timeout_args *params = (struct timeout_args*) args; //args needed for timeout

	//starting timeout and local value to make the thread wait RTO time
   	float init_timeout=params->duration*1000;
	float local_timeout;
	params->frame->time_trasm=0;
	params->frame->ripet=1;      // to recognize repeated frames (they don't contribute to the RTO calculation)

	local_timeout=init_timeout;

	for (i=0; i< cfg.max_tent; i++){
		if (cfg.coef_RTO==1){
			// doubles retransmission time
			int coef= pow(2,i);
			if (coef > COEF_MAX)
				coef=COEF_MAX; // set max value of RTO
            local_timeout=init_timeout*coef; //associate with each packet RTO right value at first retransmission
		}
		usleep(local_timeout); //thread waits amount of time previously calculated if tip_RTO = 0

		// increment of the total number of retransmissions counter
		// semaphore controls access to the tot_frame_rip global variable (used by all running timeout threads)
		int rc=pthread_mutex_lock(&mtx1);
		if (rc !=0){
   		 	fprintf(stderr," *** Error locking mutex \n");
   	 	 	exit(1);
	 	}
        tot_frame_rip++;  // counter of retransimmissions
		rc=pthread_mutex_unlock(&mtx1);
		if (rc !=0){
   		 	fprintf(stderr," *** Error unlocking mutex \n");
   	 		exit(1);
	 	}

		// random on losing probability set in configuration params
		int x=rand()%100+1;
 		if (x > cfg.prob) {
		 	// not lost packet
            fprintf(flog,"--> %d Repeated frame with sequence number        RIP %d         t = %u         repeated = %d       RTO = %.3f \n",i,params->frame->seq,params->frame->time_trasm,params->frame->ripet,local_timeout/1000);
			//send to sends data to another socket, server process socket
   			if(sendto(params->s, (char*) params->frame, sizeof(struct frame), 0, (struct sockaddr *) &(params->remote_sin), sizeof(params->remote_sin) ) < 0){
                perror(" *** Transmission failed in Timeout procedure : process ended \n\n");
      			exit(1);
            }
		}
		else{
		 	// lost packet
			fprintf(flog,"    %d Lost frame repeated with sequence number    PRP %d         t = %u         repeated = %d       RTO = %.3f \n",i,params->frame->seq,params->frame->time_trasm,params->frame->ripet,local_timeout/1000);
			// increment of the number of lost retransmissions counter
			// semaphore controls access to the tot_lost_rep_frame global variable (updated by all running timeout threads)
			rc=pthread_mutex_lock(&mtx2);
			if (rc !=0){
   		 		fprintf(stderr," *** Error locking mutex \n");
   	 	 		exit(1);
	 		}
       		tot_lost_rep_frame++;  // counter of lost frame in retransmission
			rc=pthread_mutex_unlock(&mtx2);
			if (rc !=0){
   		 		fprintf(stderr," *** Error unlocking mutex\n");
   	 	 		exit(1);
	 		}
		}
	} // endfor

    fprintf(flog,"       Exceeded maximum number of retransmissions in Timeout procedure : process ended \n\n");
	printf(" *** Exceeded maximum number of retransmissions in Timeout procedure : process ended \n\n ");
    exit(1);
 	return NULL;
}

/**************************************************************************************************
 * send_data procedure: first transmission of frame, send to server and pass to thread timeout *
**************************************************************************************************/

void* send_data(void *args){

    seqnum_t i;
    seqnum_t send_seqnum;
    int got_eof;
    struct send_file_args *state = (struct send_file_args*) args;
    uint32_t t_trasm;
    int last_frame_fs=999;

    pthread_attr_t tattr;
    int ret;
    int t_size = PTHREAD_STACK_MIN + 0x4000;  // memory reserved to single thread timeout (32 Kb)

    printf("\n\t Process %d - Transfer in progress: file/dir %s \n",getpid(),state->file_name);
  	RTO=cfg.RTO_in;
  	fprintf(flog,"       Starting value RTO = %.3f \n", RTO);

  	while(1){
        // Next sequence number expected
        send_seqnum = (state->lfs + 1) % state->seq_max;

    	// Check that it's possible to send the frame, if it's not, the thread releases the CPU.
    	if(!seq_check(((state->lar)+1) % state->seq_max, state->sws, send_seqnum, state->seq_max)){
			if (send_seqnum != last_frame_fs){
				fprintf(flog,"\n       Frame out of sequence %d \n ", send_seqnum);

				int winsup= (state->lar + state->sws)%state->seq_max;
				fprintf(flog,"       Window   winbase = %d;   winlast = %d \n ",(state->lar+1)%state->seq_max,winsup);
				tot_frame_fs++;
				last_frame_fs=send_seqnum;
            }
            sched_yield();  // set thread in ready status
      		continue;
        }
        //Frame's position in buffer
    	i = send_seqnum % BUF_SIZE;

		// reads from file (for put or get command)
    	if ((strcmp(state->istruz, "get")==0) || (strcmp(state->istruz, "put")==0))
        	got_eof = read_frame(state->f_src, &(state->frame_buf[i].send_frame), send_seqnum);
		// reads from memory ( for a list command)
		else if (strcmp(state->istruz, "list")==0)
			got_eof =read_mem(&state->path, &(state->frame_buf[i].send_frame), send_seqnum);
            // transmission time in send_file_args
     		state->frame_buf[i].send_frame.time_trasm = rtt_ts(&rttinfo);
            // thread timeout params
            state->frame_buf[i].send_frame.ripet = 1;
            state->frame_buf[i].timeout_state.s = state->s;
            state->frame_buf[i].timeout_state.duration = RTO;
            state->frame_buf[i].timeout_state.frame = &(state->frame_buf[i].send_frame);
     		state->frame_buf[i].timeout_state.remote_sin = state->remote_sin;

     		usleep(cfg.rit_send*1000);
		// size of memory of single thread timeout
		ret = pthread_attr_init(&tattr);

		if (ret!=0){
			perror(" *** Error pthread attr_init");
			printf("\n Computer resources don't support files of this size : process ended\n");
			exit(1);
        }
		ret = pthread_attr_setstacksize(&tattr,t_size);
		if (ret!=0){
			perror(" *** Error pthread attr_setstacksize");
			printf("\n Computer resources don't support files of this size : process ended\n");
			exit(1);
		}
		// creating timeout thread (for frames transmission)
        int r = pthread_create(&(state->frame_buf[i].timeout), &tattr, timeout, &(state->frame_buf[i].timeout_state));
     	if (r!=0){
			perror(" *** Error pthread create - timeout");
			printf("\n Computer resources don't support files of this size : process ended\n");
			exit(1);
        }

        // set has_ack = 0: first frame transmission, no ack received
        state->frame_buf[i].has_ack = 0;
        // update last frame sent
        state->lfs = send_seqnum;

        t_trasm=rtt_ts(&rttinfo);
     	state->frame_buf[i].send_frame.ripet = 0; //first time, never repeated
     	state->frame_buf[i].send_frame.time_trasm = t_trasm;

     	fprintf(flog,"       Transmitted frame with sequence number     TX %d          t = %u         repeated = %d \n",send_seqnum,t_trasm,state->frame_buf[i].send_frame.ripet);

     	// Transmits the current frame by simulating the loss with probability prob
    	int x=rand()%100+1;
  	 	tot_frame++;	// increase number of transmitted frames

  		if (x >= cfg.prob) {
			if(sendto(state->s, (char*) &(state->frame_buf[i].send_frame), sizeof(struct frame), 0, (struct sockaddr *) &(state->remote_sin), sizeof(state->remote_sin) ) < 0){
                perror(" *** Fail transmission in send procedure : exit from process   \n");
                exit(1);
			}
		}
   	  	else{
			tot_lost_frame++; // increase number of frames transmitted but lost on first transmission
			fprintf(flog,"       Frame lost in transmission                 TxLost %d \n", state->lfs);
        }

        if(got_eof ==1)
		break; //end of file
	}
  	if(state->f_src != NULL){
		fclose(state->f_src);
		state->f_src = NULL;
    }

  	fprintf(flog,"       OUT SEND_DATA \n");
  	return NULL;
}

/*************************************************************************************************
 * wait_ack procedure: waiting for ack, delete timeout threads, move selective repeat window *
*************************************************************************************************/

void* wait_ack(void *args){

    struct send_file_args *state = (struct send_file_args*) args;
    struct ack ack_frame;
    seqnum_t ack_seq;
    socklen_t addr_len = sizeof(state->local_sin);
    int recvlen;
    int got_eof = 0;
    uint32_t t_cor,t_trasm, t_rit, t_att;

  	srand(time(NULL));
  	RTD=cfg.rit_ack;

	// waiting cycle for answers from the receiver
  	while(1){
        // socket wating to receive message
		recvlen = recvfrom(state->s, &ack_frame, sizeof(struct ack), 0, 0, &addr_len);
    	if(recvlen <= 0)
    		continue;

		t_cor=rtt_ts(&rttinfo);  // time passed to receive

		// if receiver sends END message, the thread ends
		if  (ack_frame.type==END){
		   	fprintf(flog,"       Received END command, end of listen procedure \n");
		  	break;
        }

   		ack_seq = ack_frame.seq;
		tot_ack++; // increase number of acks

		// Simulate the loss of an ACK with cfg.prob probability set on the configuration file
		int x=rand()%100+1;
   		if (x <= cfg.prob) {
			tot_lost_ack++;
			fprintf(flog,"   >>> ACK lost before of receiving               NoAck %d \n", ack_frame.seq);
	 		continue;
	  	}
   		// frame already acked
   	 	if(state->frame_buf[ack_seq % BUF_SIZE].has_ack == 1){

            fprintf(flog,"       Received ACK with sequence number          ACK %d         trasm = %u     repeated = %d  \n",ack_seq,ack_frame.time_trasm,ack_frame.ripet);
            continue;
   		}
		if(state->frame_buf[ack_seq % BUF_SIZE].has_ack == 2){
			fprintf(flog,"       Received ACK with sequence number          ACK %d         trasm = %u     repeated = %d  \n",ack_seq,ack_frame.time_trasm,ack_frame.ripet);
           	continue;
   		}

	 	t_trasm= ack_frame.time_trasm;   // time to receive - time of transmission
	 	t_rit=t_cor-t_trasm;

	 	if (RTD > t_rit){
			t_att=(RTD-t_rit)*1000;   // inserted wait in microseconds
			usleep(t_att);
			t_cor=rtt_ts(&rttinfo);
			t_rit=t_cor-t_trasm;
		}
	  	RTD=RTD*(1+cfg.coef_RTD*1.0/100);

        fprintf(flog,"       Received ACK with sequence number          ACK %d         trasm = %u     repeated = %d  \n",ack_seq,ack_frame.time_trasm,ack_frame.ripet);

	 	// RTO update, only if it's not a retransmission
  	 	if (ack_frame.ripet==0){
			fprintf(flog,"     > Transmitted %u \n     > Received %u \n     > Delay %u \n",t_trasm,t_cor, t_rit);
			RTO=rtt_calc( &rttinfo,t_rit);
  			rtt_print(&rttinfo);
  			fprintf(flog,"       New value of RTO = %.3f \n", RTO);
	  	};

        // management of sliding window
  	 	state->frame_buf[ack_seq % BUF_SIZE].has_ack = 1;
	 	seqnum_t nfe = ((state->lar) + 1) % state->seq_max; // next frame  expected

	 	// delete timeout thread related on acked frame
	 	fprintf(flog,"       Delete thread num_seq                      CANCT %d \n", ack_frame.seq);
   	 	pthread_cancel(state->frame_buf[ack_seq % BUF_SIZE].timeout);
	 	usleep(1000);

  	 	while(state->frame_buf[nfe % BUF_SIZE ].has_ack==1){
            // if ACK is the expected one, reset has_ack
     		state->frame_buf[nfe % BUF_SIZE].has_ack = 2;
     		if(state->frame_buf[nfe % BUF_SIZE].send_frame.eof_pos >= 0)
				got_eof = 1;
     			state->lar = ((state->lar)+1) % state->seq_max;
     			nfe = ((state->lar) + 1) % state->seq_max;
		 	int winsup= (nfe+state->sws-1)%state->seq_max;
		        fprintf(flog,"   ... New window   winbase = %d;   winlast = %d \n ",nfe,winsup);
    	}
		// thread break when last ack is received
   		if(got_eof)
			break;
	}
  	fprintf(flog,"       OUT WAIT_ACK \n");
  	return NULL;
}

void transmit(struct send_file_args *state){

    int i,r;
    pthread_t send_thread;
    pthread_t ack_thread;
    uint32_t t_cor;

	for(i = 0; i < BUF_SIZE; i++){
		state->frame_buf[i].has_ack = 0; // at start, no frame is acked
	}

	rtt_init(&rttinfo);
  	t_cor=rtt_ts(&rttinfo);
   	fprintf(flog,"\n       Start process time %u \n",t_cor);
  	rtt_print(&rttinfo);

	r = pthread_create(&send_thread, NULL, send_data, (void*) state);
	if (r!=0){
		perror(" *** Error pthread create - send_data");
		printf("\n\t Process ended\n");
		exit(1);
  	}
	r = pthread_create(&ack_thread, NULL, wait_ack, (void*) state);
    if (r!=0){
		perror(" *** Error pthread create - wait_ack");
		printf("\n\t Process ended\n");
		exit(1);
   	}
	// waits the two threads created to proceed, they will operate and end at transmission completed
	pthread_join(send_thread, NULL);
 	pthread_join(ack_thread, NULL);

	// writes summary data to the log file
	fprintf(flog, "\n ================================================================================================================ \n");
	fprintf(flog, " TRANSMISSION STATS \n ");
	fprintf(flog, " total frame                            : %d  \n ", tot_frame);
	fprintf(flog, "   of which lost at first transmission  : %d  \n ", tot_lost_frame);
	fprintf(flog, " repeated frame                         : %d  \n ", tot_frame_rip);
	fprintf(flog, "   of which lost                        : %d  \n ", tot_lost_rep_frame);
	fprintf(flog, " ack product (remotely)                 : %d  \n ", tot_ack);
	fprintf(flog, "   of which lost                        : %d  \n ", tot_lost_ack);
	fprintf(flog, " frame out of sequence                  : %d  \n ", tot_frame_fs);
	fprintf(flog, " duration of the process                : %u  \n ", rtt_ts(&rttinfo));
	fprintf(flog, "================================================================================================================= \n\n");

	printf("\t Process %d - Transfered file/dir: %s\n ",getpid(),state->file_name);

}

void receive(struct send_file_args *state){

    struct frame frame;
    struct ack ack_frame;
    struct frame frame_buf[BUF_SIZE];
    int got_eof=0;
    int last_frame_fs=999;
    uint32_t t_cor;

	printf("\n\t Process %d - Reception in progress: file/dir %s \n",getpid(),state->new_filename);
	fprintf(flog,"       IN RECEIVE \n");

	// initialize reception process start time
	rtt_init(&rttinfo);
  	t_cor=rtt_ts(&rttinfo);
   	fprintf(flog,"\n       Starting time process %u \n",t_cor);

	socklen_t   remote_sinlen=sizeof(state->remote_sin);

	for(int i = 0; i < BUF_SIZE; i++){
  		frame_buf[i].type = EMPTY; //receiving frame buffer
 	}

	while(1){
		while(1){
			// waiting for a frame arrive
			fprintf(flog, "t= %d :\n       Waiting for a frame arrive \n",rtt_ts(&rttinfo));
			TMax.tv_sec = cfg.t_max;
			TMax.tv_usec = 0;
			FD_ZERO(&rset);
        	FD_SET(state->s, &rset);
       		maxfdp = state->s+1;

			int n=select(maxfdp, &rset, NULL, NULL, &TMax);
			if(n==0){
				fprintf(flog,"t= %d :\n       Maximum waiting time exceeded \n",rtt_ts(&rttinfo));
				printf("\n Child %d : maximum waiting time exceeded - end of process \n", getpid());
				exit(EXIT_FAILURE);
			}
			if (FD_ISSET(state->s, &rset)){
				int l=recvfrom(state->s, &frame, sizeof(struct frame), 0,
  	 				(struct sockaddr *) &state->remote_sin, & remote_sinlen) ;

				if (l <= 0)
					continue;
				else break;
			}
		}//second while ended

		fprintf(flog,"t= %d :\n",rtt_ts(&rttinfo));
		fprintf(flog,"       Received frame with sequence number        %d \n",frame.seq);
        ric_tot_frame_pv++;

    	ack_frame.type = ACK;
   		ack_frame.seq = frame.seq;
   		ack_frame.time_trasm=frame.time_trasm;
   		ack_frame.ripet=frame.ripet;

        // sending ack for each frame
		if(sendto(state->s, (char*) &ack_frame, sizeof(struct ack), 0,(struct sockaddr *) &state->remote_sin, sizeof(state->remote_sin))< 0){
            perror(" *** Transmission failed in receive procedure - end of process\n");
    		exit(1);
  		}
		fprintf(flog,"       ACK sent with sequence number              %d \n",ack_frame.seq);

  		if(!seq_check((state->lar+1) % state->seq_max, state->sws, frame.seq, state->seq_max)){
			if (frame.seq != last_frame_fs){
				fprintf(flog,"       Received frame out of sequence             %d \n", frame.seq); //frame out of sequence
				int winsup= (state->lar + state->sws)%state->seq_max;
				fprintf(flog,"   ... Window   winbase = %d;   winlast = %d \n\n ",(state->lar+1)%state->seq_max,winsup);
				ric_tot_frame_fs++;
				last_frame_fs=frame.seq;
    		}
			continue;
		}

  		frame_buf[frame.seq % BUF_SIZE] = frame;

 		//while other frame in buffer has new winbase sequence number, do the same process to them, till the last one consecutive in buf
 		if(frame.seq == (state->lar+1)%state->seq_max){
     		do{
                state->lar = (state->lar+1)%state->seq_max;
				if(frame_buf[state->lar%BUF_SIZE].type == DATA){
					//if winbase, set the buffer position to empty, then write process
					frame_buf[state->lar%BUF_SIZE].type = EMPTY;
					if ((strcmp(state->istruz, "get")==0) || (strcmp(state->istruz, "put")==0))
    						got_eof = write_frame(state->f_dest, &(frame_buf[state->lar%BUF_SIZE]));
					else
						got_eof = write_mem(state->path, &(frame_buf[state->lar%BUF_SIZE]));
					ric_tot_frame++;
				}

    		}while(frame_buf[((state->lar+1)%state->seq_max)%BUF_SIZE].type == DATA);

   			int winsup= (state->lar+1+state->sws-1)%state->seq_max;
			fprintf(flog,"   ... New window   winbase = %d;   winlast = %d \n \n",state->lar+1,winsup);
		}

     	if(got_eof == 1){
   			if(state->f_dest != NULL){
				fclose(state->f_dest); //close at last frame
				state->f_dest = NULL;
    		}
            if ((strcmp(state->istruz, "get")==0)){
                printf("\t Process %d - Received file: %s \n",getpid(),state->new_filename);
                printf("\n Enter a command: >> ");
                break;
            }
            if ((strcmp(state->istruz, "list")==0)){
                printf(" DIRECTORY CONTENT:  \n ");
                printf(" --> %s \n ", state->path);
                break;
            }
  		}
	} // first while ended

	// message of end reception
	ack_frame.type = END;
	if(sendto(state->s, (char*) &ack_frame, sizeof(struct ack), 0,(struct sockaddr *) &state->remote_sin, sizeof(state->remote_sin))< 0){
   		perror(" *** Transmission failed in receive procedure - end of process \n");
 		exit(1);
  	}
	fprintf(flog,"\n ================================================================================================================ \n");
	fprintf(flog,"  RECEPTION STATS \n ");
	fprintf(flog,"  total received frames/ transmitted ack  : %d \n ", ric_tot_frame_pv);
	fprintf(flog,"  frames out of sequence                  : %d \n ", ric_tot_frame_fs);
	fprintf(flog,"  total number of written frames          : %d \n ", ric_tot_frame);
	fprintf(flog,"================================================================================================================= \n\n");
}

#endif
