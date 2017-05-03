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
#define BUFLEN 1200
#define KB 1024
typedef char bool;
enum{
	false, true
};
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
int udpfd;
int agent_sz;
int recv_len;
int bufSize;
int base;
char buf[BUFLEN+1];
bool *buf_mask;
int buf_cnt;
int buf_arr_cnt;
char *buf_arr;
struct sockaddr_in clientAddress, agentAddress;
FILE *fp;
void initBufStat(){
	buf_arr_cnt=0;
	memset(buf_arr,0,sizeof(char)*(KB*bufSize+2));
	memset(buf_mask,0,sizeof(bool)*(bufSize+1));
	buf_cnt=0;
}
void writeBufArr(){
	fwrite(buf_arr,sizeof(char),buf_arr_cnt,fp);
	fflush(fp);
	printf("fflush\n");
	fflush(stdout);
}
int main(int argc, char *argv[]){ //clientPort, agentPort, file
	if(argc!=4)
		die("argument");
	fp=fopen(argv[3],"wb+");
	if((udpfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1)
		die("socket");
	bzero(&clientAddress, sizeof(clientAddress));
	clientAddress.sin_family = AF_INET;
	clientAddress.sin_port = htons(atoi(argv[1]));
	clientAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	if((bind(udpfd, (struct sockaddr*)&clientAddress, sizeof(clientAddress)))==-1)
		die("bind");
	agentAddress.sin_family=AF_INET;
	agentAddress.sin_port = htons(atoi(argv[2]));
	agentAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	agent_sz=sizeof(agentAddress);
	bufSize=32;
	base=0;
	buf_arr=malloc(sizeof(char)*(KB*bufSize+2));
	buf_mask=malloc(sizeof(bool)*(bufSize+2));
	initBufStat();
	while(1){	
		recv_len = recv(udpfd, buf, BUFLEN, 0);
		if(recv_len==-1)
			die("recv");
		//deal with data
		struct Data data;
		memcpy(&data,buf,sizeof(data));
		if(strcmp(data.type,"fin")==0){  //is fin
			printf("recv    %-4s\n",data.type);
			fflush(stdout);
			struct Ack ack;
			strcpy(ack.type,"finack");
			memcpy(buf,&ack,sizeof(ack));
			if (sendto(udpfd, buf, sizeof(ack), 0, (struct sockaddr*)&agentAddress, agent_sz) == -1)
				die("sendto");
			printf("send    %-4s\n",ack.type);
			fflush(stdout);
			//write not-written datas
			writeBufArr();
			break; // suppose finack won't drop :P
		}else{  //is data
			if(data.id>base+bufSize){ //overflow
				if(buf_cnt==bufSize){ //buf is full, can write to file
					writeBufArr();
					initBufStat();
					base+=bufSize;
				}else{
					printf("drop    %-4s    #%d\n",data.type,data.id);
					fflush(stdout);
					continue;
				}
			}
			if(data.id>base){ // in current buffer
				if(buf_mask[data.id-base]==0){ // new
					buf_mask[data.id-base]=1;
					buf_cnt++;
					buf_arr_cnt+=data.payload_size;
					memcpy(buf_arr+(data.id-base-1)*KB,data.payload,data.payload_size);
					printf("recv    %-4s    #%d\n",data.type,data.id);
					fflush(stdout);
				}else{ // received before
					printf("ignr    %-4s    #%d\n",data.type,data.id);
					fflush(stdout);
				}
			}else{ // prior than current buffer
				printf("recv    %-4s    #%d\n",data.type,data.id);
				fflush(stdout);
			}
			//send ack
			struct Ack ack;
			ack.id=data.id;
			strcpy(ack.type,"ack");
			memcpy(buf,&ack,sizeof(ack));
			if (sendto(udpfd, buf, sizeof(ack), 0, (struct sockaddr*)&agentAddress, agent_sz) == -1)
				die("sendto");
			printf("send    %-4s    #%d\n",ack.type,ack.id);
			fflush(stdout);
		}
	}
	fclose(fp);
	close(udpfd);
	return 0;
}
