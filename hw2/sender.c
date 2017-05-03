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
#define RESEND 789
#define SEND 567
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
void handler(){}
int max(int a,int b){
	return a>b?a:b;
}
int udpfd;
int recv_len,agent_sz;
int threshold, winSize, base;
int canSend;
bool *ack_mask;
bool *send_mask;
bool finish_data, wait_for_finack;
int ack_cnt;
int file_sz;
char buf[BUFLEN+1];
struct sockaddr_in serverAddress, agentAddress;
FILE *fp;
size_t res;
void initAckStat(){
	free(ack_mask);
	ack_mask=malloc(sizeof(bool)*(winSize+2));
	ack_cnt=0;
	for(int i=1;i<=winSize;i++){
		send_mask[base+i+1]=false;
		ack_mask[i]=false;
	}
}
int getNewBase(){ // only called when time out
	for(int i=1;i<=winSize;i++)
		if(ack_mask[i]==0) return base+i-1;
	return base+winSize;
}
void sendData(int type){
	if(finish_data) return ;
	for(int i=0;i<winSize;i++){
		struct Data data;
		strcpy(data.type,"data");
		data.id=base+i+1;
		fseek(fp, (data.id-1)*KB, SEEK_SET );
		res=fread(&data.payload,1,KB,fp);
		data.payload_size=res;
		if(res!=KB){
			finish_data=true; // finish sending;
			winSize=i+1;
		}
		memcpy(buf,&data,sizeof(data));
		if (sendto(udpfd, buf, sizeof(data), 0, (struct sockaddr*)&agentAddress, agent_sz) == -1)
			die("sendto");
		if(type==RESEND||send_mask[data.id]) printf("resnd   %-4s  #%d,     winSize = %d\n",data.type,data.id,winSize);
		else printf("send    %-4s  #%d,     winSize = %d\n",data.type,data.id,winSize);
		fflush(stdout);
		send_mask[data.id]=true;
	}
	//start timer
	alarm(1);
}
void sendFin(int type){
	struct Data data;
	strcpy(data.type,"fin");
	memcpy(buf,&data,sizeof(data));
	data.payload_size=0;
	if (sendto(udpfd, buf, sizeof(data), 0, (struct sockaddr*)&agentAddress, agent_sz) == -1)
		die("sendto");
	if(type==RESEND) printf("resnd   %-4s\n",data.type);
	else printf("send    %-4s\n",data.type);
	//start timer
	alarm(1);
}
int main(int argc, char* argv[]){ //serverPort, agentPort, file
	if(argc!=4)
		die("argument");
	fp=fopen(argv[3],"rb");
	fseek(fp, 0L, SEEK_END);
	file_sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	send_mask=malloc(sizeof(bool)*(file_sz/KB+4));
	if((udpfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1)
		die("socket");
	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(atoi(argv[1]));
	serverAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	if((bind(udpfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)))==-1)
		die("bind");
	bzero(&agentAddress, sizeof(agentAddress));
	agentAddress.sin_family=AF_INET;
	agentAddress.sin_port = htons(atoi(argv[2]));
	agentAddress.sin_addr.s_addr=inet_addr("127.0.0.1");
	agent_sz=sizeof(agentAddress);
	threshold=16;
	winSize=1;
	base=0; //window not include base, window=base+1....
	canSend=1;
	finish_data=false;
	wait_for_finack=false;
	siginterrupt(SIGALRM, 1);
	signal(SIGALRM,handler); 
	while(1){
		if(canSend){  //send
			initAckStat();
			sendData(SEND);
			canSend=0;
		}
		recv_len = recv(udpfd, buf, BUFLEN, 0);
		if(recv_len == -1){
			if(errno == EINTR){  //timeout, resend
				if(wait_for_finack){
					sendFin(RESEND);
				}else{
					base=getNewBase();
					//reset threshold and winSize
					threshold=max(winSize/2,1);
					winSize=1;
					printf("time    out,          threshold = %d\n",threshold);
					initAckStat();
					if(finish_data) finish_data=false;
					sendData(RESEND);
				}
			}else{
				die("recv");
			}
		}else{  //recieve ack
			struct Ack ack;
			memcpy(&ack,buf,sizeof(ack));
			if(strcmp(ack.type,"finack")==0){ // receive fin_ack
				alarm(0); //stop timer
				printf("recv    %-4s\n",ack.type);
				fflush(stdout);
				break;
			}
			printf("recv    %-4s  #%d\n",ack.type,ack.id);
			fflush(stdout);
			if(ack.id>base){
				if(ack_mask[ack.id-base]==0) ack_cnt++;
				ack_mask[ack.id-base]=1;
			}
			if(ack_cnt==winSize){ //current window is full (without timeout)
				canSend=1;
				base+=winSize;
				alarm(0); //stop timer
				//increase window size of next round
				if(winSize<threshold) winSize*=2;
				else winSize++;
				//finish sending and recieve ack, send fin
				if(finish_data){ 
					sendFin(SEND);
					wait_for_finack=true;
				}
			}
		}
	}
	fclose(fp);
	close(udpfd);
	return 0;
}
