#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#define MAXLINE 4096 
#include <cJSON.h>
int sockfd;
static char* ftp_ip=NULL;
static char* ftp_name=NULL;
static  char* ftp_pwd=NULL;
//static  char* server;
//static  int port;
char * ftp(char*,char*,char*,char*,int);
char* request(char param,char* url){
    cJSON * pJsonRoot = NULL;
    pJsonRoot = cJSON_CreateObject();
    if(NULL == pJsonRoot)
    {
        //error happend here
        return NULL;
    }
    cJSON_AddStringToObject(pJsonRoot, "version", "1.0.0");
    cJSON_AddStringToObject(pJsonRoot, "messageid", "消息ID");
    cJSON_AddStringToObject(pJsonRoot, "mode", "request");
    cJSON_AddStringToObject(pJsonRoot, "robotid", "机器人SN");
    cJSON_AddStringToObject(pJsonRoot, "targetID", "4");
    cJSON_AddStringToObject(pJsonRoot, "sourceID", "6");
    switch(param){
	    case 'a':
    		cJSON_AddStringToObject(pJsonRoot, "orderType","pdaHeart" );
		break;
	    case 'b':
   		 cJSON_AddStringToObject(pJsonRoot, "orderType","person_detect");
		 break;
    }
    if(NULL!=url){
    cJSON_AddStringToObject(pJsonRoot, "param", url);
    }
    char * p = cJSON_Print(pJsonRoot);
    cJSON_Delete(pJsonRoot);
    return p;
}
char* getParam(char* pMsg){
    if(NULL == pMsg)
    {
        return NULL;
    }
    cJSON * pJson = cJSON_Parse(pMsg);
    cJSON * pSub = cJSON_GetObjectItem(pJson, "param");
    cJSON * pSubSub = cJSON_GetObjectItem(pSub, "thresh");
    if(NULL == pSubSub)
    {
        // get object from subject object faild
    }
    printf("sub_obj_1 : %s\n", pSubSub->valuestring);
    int len=strlen(pSubSub->valuestring);

    char * p = malloc(len);
   // strncmp(pSubSub->valuestring,p,len);
    cJSON_Delete(pJson);
    return p;
}
void setupSocket(char* server,int port,char* ftp_ip_,char* ftp_name_,char* ftp_pwd_){
    ftp_ip=ftp_ip_;
    ftp_name=ftp_name_;
    ftp_pwd=ftp_pwd_;

    int rec_len;  
    //char    recvline[4096], sendline[4096];  
    char    buf[MAXLINE];  
    struct sockaddr_in    servaddr;  
  
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){  
    printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);  

   // exit(0);  
    }  
  
  
    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;  
    servaddr.sin_port = htons(port);  
    if( inet_pton(AF_INET,server, &servaddr.sin_addr) <= 0){  
    printf("inet_pton error for %s\n",server);  
   // exit(0);  
    }  
  
  
    if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){  
    printf("connect error: %s(errno: %d)\n",strerror(errno),errno);  
  //  exit(0); a
     sockfd=-1;
      return; 
    }  
  
  
    printf("send msg to server: \n");  
    //fgets(sendline, 4096, stdin);  
    char * sj=request("loginMainServer",NULL);
    printf("sj finish\n");
    if( send(sockfd, sj, strlen(sj), 0) < 0){
    printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);  
 //   exit(0);  
    } 


    printf("sj1 finish\n");


    if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {  
       perror("recv error");  
//       exit(1);  
    }

    printf("sj2 finish\n");




    buf[rec_len]  = '\0';  
    free(sj);
    printf("socket finish\n");
}
void sendData(char* data,int size,float thresh)
{
    char retData='a';
    char* url=NULL;

    if(NULL!=data&&size>0){
	url = ftp(ftp_ip,ftp_name,ftp_pwd,data,size);
	retData='b';
    }

	if(-1==sockfd)
		return;
      char * sj=request(retData,url);
      if( send(sockfd, sj, strlen(sj), 0) < 0){
       printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);  
      // exit(0);  
      }
      free(sj);  
    fd_set fdR;
    FD_ZERO(&fdR); 
    FD_SET(sockfd, &fdR);
    struct timeval timeout;
   timeout.tv_usec=10;
   timeout.tv_sec=0;
   // int rec_len=0;
    switch (select(sockfd + 1, &fdR, NULL,NULL, &timeout)) { 
    case -1:printf("selet error\n");break; 
    case 0: break;
    default: 
       if (FD_ISSET(sockfd,&fdR)) {
       /*	      char buf[MAXLINE]; 
          if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {  
             perror("recv error");  
           //  exit(1);
	  }
	   char * para = getParam(&buf[0]);
           int per=atoi(para);
	   demo_thresh=per/100.00;
	   free(para);*/
       } 
    
    }
   printf("socket finsh\n"); 
}

void destroy(){
   if(close(sockfd) < 0)
        printf("close error\n");
}