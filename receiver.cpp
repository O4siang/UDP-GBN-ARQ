//receiver.cpp
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
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <zlib.h>


#define PORT 15004

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

struct ackPacket createSynAckPkt(int seq_no);
struct ackPacket createDataAckPkt(int seq_no);
struct ackPacket createFinAckPkt(int seq_no);
void GBNUDP_receive();

int main() {
	
	while(1) {
		GBNUDP_receive();	
	}
    return 0;
}

void GBNUDP_receive() {

	char str[1024]; //initailize string receive buffer
	for (int i = 0; i<1024; i++) 
		str[i] = '\0';
	
	//set up UDP socket structure
	int sockfd;
	char *re = str;
    char buffer[256];
    socklen_t receiver_len, sender_len;
    struct sockaddr_in sender_addr;   
    struct sockaddr_in receiver_addr;
    struct in_addr sender_ip_addr;
    
    bzero(&receiver_addr, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    receiver_addr.sin_port = htons(PORT);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    receiver_len = sizeof(receiver_addr);
    sender_len = sizeof(sender_addr);
	
	//set up timer for FIN packet control 5000000ns = 5ms
	struct timespec t1;
	t1.tv_sec = 0;
	t1.tv_nsec = 5000000L;


	if(bind(sockfd, (struct sockaddr *)&receiver_addr, receiver_len) < 0) {
		perror ("bind failed\n");
		exit(1);
	}

	//set socket as non-block mode
	int flags = fcntl(sockfd, F_GETFL, 0);
  	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	printf("Receiver is running and ready to receive connections on port 15004...\n");
	
	int seq = 0;
	int waitSeq = 0;
	int state = 0;
	int finFlag = 0;
	int synFlag = 0;
	int closeFlag = 1;
	int wait = 0;
	
	while(closeFlag) {
		
		//if fin packet receive, start the timer till 5 seconds and close socket
		if (finFlag) {
        	++wait;
        	if (wait > 1000) {
        		printf("Closed timed-up...\n");
        		closeFlag = 0;
        		close(sockfd);
        	}
        	nanosleep(&t1, NULL);
        }

    	
        struct dataPacket packet;
        struct ackPacket ack;

        //execution as stop and wait, but we need the non-block mode for measuring time
        //If receive nothing-> do nothing, else send back the packet with ACK and SEQ
    	int status;
        status = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr, &sender_len);
        if(status < 0) {
      
        } else {
        	if (packet.type == '2' && packet.seq == 0) {
        		memcpy(&sender_ip_addr, &sender_addr.sin_addr.s_addr, 4);
  				printf("Connection request received from <%s:%d#>\n", inet_ntoa(sender_ip_addr), ntohs(sender_addr.sin_port));
  				ack = createSynAckPkt(seq);
  				seq = packet.seq;
  				waitSeq = seq + 1;
  				printf("Sending ACK with SEQ # %d, expecting SEQ # %d\n", seq, waitSeq);
  				sendto(sockfd, &ack, sizeof(ack), 0,(struct sockaddr *)&sender_addr, sizeof(sender_addr));
        		synFlag = 1;
        	} else if (packet.seq > 0 && packet.type == '0' && synFlag){
        		if (packet.seq == waitSeq) {
        			waitSeq++;
        			seq = packet.seq;
        			str[seq-1] = packet.data[0];
        			//printf("%s\n", str);
        			ack = createDataAckPkt(seq);
        			printf("Received character \"%c\"\n", packet.data[0]);
        			printf("Sending ACK with SEQ # %d, expecting SEQ # %d\n", seq, waitSeq);
        			sendto(sockfd, &ack, sizeof(ack), 0,(struct sockaddr *)&sender_addr, sizeof(sender_addr));

       			} else if (packet.seq != waitSeq) {
       				printf("Out of order SEQ # %d\n", packet.seq);
       				printf("Sending ACK with SEQ # %d\n", waitSeq-1);
       				ack = createDataAckPkt(waitSeq-1);
       				sendto(sockfd, &ack, sizeof(ack), 0,(struct sockaddr *)&sender_addr, sizeof(sender_addr));

       			}

        	} else if (packet.type == '1' && synFlag) {
        		ack = createFinAckPkt(packet.seq);
        		if (finFlag == 0) {
        			cout << "Sender is terminating with FIN..." << endl;
					cout << "Sending ACK with SEQ # " << packet.seq << endl;
        			sendto(sockfd, &ack, sizeof(ack), 0,(struct sockaddr *)&sender_addr, sizeof(sender_addr));
        			cout << "Reception Complete: "; 
					printf("%s\n", str);
        		}
        		if (finFlag == 1) {
        			cout << "Sender is terminating with FIN..." << endl;
					cout << "Sending ACK with SEQ # " << packet.seq << endl;
        			sendto(sockfd, &ack, sizeof(ack), 0,(struct sockaddr *)&sender_addr, sizeof(sender_addr));
        		}
        		
        		finFlag = 1;
				
        	}

    	}
        
        
    }
    
}

//function for creating ack packet
struct ackPacket createSynAckPkt(int seq_no) {
	struct ackPacket pkt;
	pkt.type = '6';
	pkt.seq = seq_no;
	
	return pkt;
};

struct ackPacket createDataAckPkt(int seq_no) {
	struct ackPacket pkt;
	pkt.type = '4';
	pkt.seq = seq_no;
	
	return pkt;
};

struct ackPacket createFinAckPkt(int seq_no) {
	struct ackPacket pkt;
	pkt.type = '5';
	pkt.seq = seq_no;
	
	return pkt;
};