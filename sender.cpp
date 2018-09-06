//sender.cpp
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <time.h>
#include <zlib.h>

#define PORT 15004 //setting port
#define WINDOW 5 //setting window size

using namespace std;

/*
Define packet bit structure and type
ACK  SYN  FIN  SEQ  DATA
0    1    0    0    --   -->first packet for handshaking: type = 010 (2)
1    1    0    0    --   -->ack from receiver, establish connection: type = 110 (6)
0    0    0    int  char -->start sending packet with data: type = 000 (0)
1    0    0    int  --   -->ack from receiver, without data: type = 100 (4)
0    0    1    int  --   -->data transmission completed, send packet with FIN: type = 001 (1)
1    0    1    int  --   -->terminating acked packet, close connection: type = 101 (5)
*/

struct dataPacket
{
	char type; 
	int seq;
	char data[2];
};

struct ackPacket
{
	char type;
	int seq;	
};

void GBNUDP_send(char* data);
struct dataPacket createSynPkt(int seq_no);
struct dataPacket createDataPkt(int seq_no, char* data);
struct dataPacket createFinPkt(int seq_no);

//pointer for Seq and Ack position
int curSeq = 0;
int curAck = 0;


int main() {
	struct timespec start;
	struct timespec end;

	while (1) {
		cout << "Please provide a string with at least 10 characters:" << endl;
		char *input;
		scanf("%s", input);
		//1000 bytes experiment for extra question 2
		//char *input = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
		//timespec_get(&start, TIME_UTC);
		GBNUDP_send(input);
		//timespec_get(&end, TIME_UTC);
		//printf("%lld.%.9ld\n", (long long)(end.tv_sec-start.tv_sec), (end.tv_nsec-start.tv_nsec));
		break;
	}
	
    return 0; 
}

void GBNUDP_send(char* data) {
	
	int lastSeq = strlen(data); //count input length
	bool ackArray[lastSeq+1];
	
	//ininitailize ack record array
	for (int i = 0; i <=lastSeq; i++) {
		ackArray[i] = true;
	}

	//set up timer time interval => 10000000 ns = 10ms
	struct timespec t1;
	t1.tv_sec = 0;
	t1.tv_nsec = 10000000L;

	//set up UDP socket configuration
	int sockfd;
	socklen_t receiver_len, sender_len;
    struct sockaddr_in sender_addr;   
    struct sockaddr_in receiver_addr;

    bzero(&receiver_addr, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = inet_addr("10.0.0.2");
    receiver_addr.sin_port = htons(PORT);

    sender_addr.sin_port = htons(PORT);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    receiver_len = sizeof(receiver_addr);

	if (sockfd < 0) {
		perror ("socket failed!");
		exit(1);
	}
    
    //Make the socket execute as non-block mode
    int flags = fcntl(sockfd, F_GETFL, 0);
  	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

  	int state = 0;
  	int recv;
  	int wait = 0;
  	int loopflag = 1;

  	//make 10ms delay per loop, implement GBN resend per 20 loop
  	while(loopflag) {
  		nanosleep(&t1, NULL);
  		struct dataPacket pkt;
  		struct ackPacket ack;

  		switch(state) {
  			//state at handshaking
  			case 0:
  				pkt = createSynPkt(0);
  			    sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
  			    printf("Establishing a connection to receiver... (sending SYN)\n");
  			    state++;
  			    break;

  			// if handshaking complete, move on. Or resend Syn packet
  			case 1:
  				recv =recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &receiver_len);
				if (recv < 0) {
					++wait;
  				}
  				else {
  					++wait;
  					if (ack.seq == 0 && ack.type == '6') {
  						printf("ACK Received with SEQ # %d\n", ack.seq);
  						curSeq = 1;
  						curAck = 1;
  						wait = 0;
  						state++;
  					}
  					
  				}
  				//printf("%d\n",wait);
				if (wait > 20) {
					printf("SYN timed-out...\n");
					wait = 0;
					state = 0;
				}
  				break;
  			// start sending packet with data, if received acked packet, Seq pointer move on to ack.seq+1, timer reset to 0 
  			// if loop > 20, time out and resend packet by Go-Back-N
  			case 2:
  				if (curSeq <= lastSeq){
					if (((curSeq - curAck) < WINDOW) && ackArray[curSeq]) {
  						pkt = createDataPkt(curSeq, data+curSeq-1);
  						printf("Sending character \"%c\"\n", pkt.data[0]);
  						sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
  						ackArray[curSeq] = false;
  						curSeq++;
  					}
  				} 

  				recv = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &receiver_len);
				if (recv < 0) {
					++wait;
  				} else {
  					++wait;
					if (curAck <= ack.seq) {
						printf("ACK Received with SEQ # %d\n", ack.seq);
						for (int i = curAck; i <= ack.seq ; i++) {
							ackArray[i] = true;
						}
						curAck = (ack.seq + 1);
						wait = 0;
					}
  					
  				}

  				if (curAck > lastSeq) {
  					state++;
  					pkt = createFinPkt(curAck);
  					printf("Terminating connection... (sending FIN)\n");
  			    	sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
  				}
  				//printf("%d\n",wait);
				if (wait > 20) {
					printf("Window timed-out...\n");
					wait = 0;
					curSeq = curAck;
					if ((lastSeq - curAck) < (WINDOW - 1)) {
						for (int i = 0; i <= (lastSeq - curAck); i++) {
							ackArray[curAck + i] = true;
						}
					} else if (lastSeq - curAck >= (WINDOW - 1)) {
						for (int i = 0; i< WINDOW; i++) {
							ackArray[curAck + i] = true;
						}
					}
						
				}
  				break;

  			//sending packet with FIN packet,keep resend every 200ms until receive Acked FIN
  			//if after 5 sencond and not received Acked Fin, close socket
  			case 3:
  			  	recv =recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiver_addr, &receiver_len);
  				if (recv < 0) {
  					++wait;
  				} else {
  					++wait;
  					if (ack.seq == (lastSeq+1) && ack.type == '5') {
  						 wait = 1;
  						 printf("ACK Received with SEQ # %d\n", ack.seq);
  						 printf("Done!\n");
  						 close(sockfd);
  						 loopflag = 0;
  					}
  				}
  				if (wait > 499) {
					printf("Done!\n");
  					close(sockfd);
  					loopflag = 0;
  				} else if (wait%20 == 0) {
  					printf("FIN timed-out...\n");
  					pkt = createFinPkt(curAck);
  					printf("Terminating connection... (sending FIN)\n");
  					sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
  				}
  				
  				break;
  		}
  	}
    
}

// function for creating packet
struct dataPacket createSynPkt(int seq_no) {
	struct dataPacket pkt;
	pkt.type = '2';
	pkt.seq = seq_no;
	memset(pkt.data, 0, sizeof(pkt.data));
    
    return pkt;

}

struct dataPacket createDataPkt(int seq_no, char* data) {
	struct dataPacket pkt;
	pkt.type = '0';
	pkt.seq = seq_no;
	memset(pkt.data, 0, sizeof(pkt.data));
	pkt.data[0] = data[0];
	pkt.data[1] = '\0';
    return pkt;
}

struct dataPacket createFinPkt(int seq_no) {
	struct dataPacket pkt;
	pkt.type = '1';
	pkt.seq = seq_no;
	memset(pkt.data, 0, sizeof(pkt.data));

	return pkt;
}