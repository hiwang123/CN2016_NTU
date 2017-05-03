#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#define BUFLEN 1200
#define KB 1024
struct Ack{
	int id;
	char type[10];
};
struct Data{
	int id;
	char type[10];
	int payload_size;
	char payload[KB+2];
};
void die(char *s){
	perror(s);
	exit(1);
}
char buf[BUFLEN+1];
int udpfd;
int server_sz,client_sz,recv_sz;
int recv_len;
int total_pkt, dropped_pkt;
int rand_num;
struct sockaddr_in serverAddress, agentAddress, clientAddress, recvAddress;
int main(int argc, char* argv[]){  //agentPort, serverPort, clientPort, 0~10(loss rate)
	if(argc!=5)
		die("argument");
	if((udpfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1)
		die("socket");
	bzero(&agentAddress, sizeof(agentAddress));
	agentAddress.sin_family = AF_INET;
	agentAddress.sin_port = htons(atoi(argv[1]));
	agentAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	if((bind(udpfd, (struct sockaddr*)&agentAddress, sizeof(agentAddress)))==-1)
		die("bind");
	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family=AF_INET;
	serverAddress.sin_port = htons(atoi(argv[2]));
	serverAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	server_sz=sizeof(serverAddress);
	bzero(&clientAddress, sizeof(clientAddress));
	clientAddress.sin_family=AF_INET;
	clientAddress.sin_port = htons(atoi(argv[3]));
	clientAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	client_sz=sizeof(clientAddress);
	recv_sz=sizeof(recvAddress);
	total_pkt=dropped_pkt=0;
	srand(time(NULL));
	while(1){
		recv_len=recvfrom(udpfd, buf, BUFLEN, 0, (struct sockaddr *) &recvAddress, &recv_sz);
		if(recv_len==-1)
			die("recvfrom");
		if(recvAddress.sin_port==htons(atoi(argv[2]))){ //recv from server
			total_pkt++;
			struct Data data;
			memcpy(&data,buf,sizeof(data));
			if(strcmp(data.type,"fin")==0){  //recv fin
				printf("get     %-4s\n",data.type);
				fflush(stdout);
				if (sendto(udpfd, buf, sizeof(data), 0, (struct sockaddr*)&clientAddress, client_sz) == -1)
					die("sendto(server)");
				printf("fwd     %-4s\n",data.type);
				fflush(stdout);
			}else{     //recv data
				printf("get     %-4s    #%d\n",data.type,data.id);
				fflush(stdout);
				rand_num=rand()%10;
				if(rand_num>=atoi(argv[4])){ // forward
					if (sendto(udpfd, buf, sizeof(data), 0, (struct sockaddr*)&clientAddress, client_sz) == -1)
						die("sendto(server)");
					printf("fwd     %-4s    #%d,     loss rate = %.4f\n",data.type,data.id,(double)dropped_pkt/total_pkt);
					fflush(stdout);
				}else{ // drop
					dropped_pkt++;
					printf("drop    %-4s    #%d,     loss rate = %.4f\n",data.type,data.id,(double)dropped_pkt/total_pkt);
					fflush(stdout);
				}
			}
		}else if(recvAddress.sin_port==htons(atoi(argv[3]))){ //recv from client
			total_pkt++;
			struct Ack ack;
			memcpy(&ack,buf,sizeof(ack));
			if(strcmp(ack.type,"finack")==0){ //recv finack
				printf("get     %-4s\n",ack.type);
				fflush(stdout);
				if (sendto(udpfd, buf, sizeof(ack), 0, (struct sockaddr*)&serverAddress, server_sz) == -1)
					die("sendto(client)");
				printf("fwd     %-4s\n",ack.type);
				fflush(stdout);
				break; // suppose finack won't drop
			}else{ //recv ack
				printf("get     %-4s    #%d\n",ack.type,ack.id);
				fflush(stdout);
				if (sendto(udpfd, buf, sizeof(ack), 0, (struct sockaddr*)&serverAddress, server_sz) == -1)
					die("sendto(client)");
				printf("fwd     %-4s    #%d\n",ack.type,ack.id);
				fflush(stdout);
			}
		}else{
			die("port");
		}
	}
	close(udpfd);
	return 0;
}
