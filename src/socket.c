#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>
#define MAXLINE 4096
#define CHAR20 20
#include <cJSON.h>
#include "verifyutil.h"
#include "list.h"
#include "image.h"
int sockfd;
int socket_sec = 0;
int socket_usec = 10;
//char  servertime[30] = "20160212";
static char *robotIDg = (char *) "";
float threshg = 0;
static char ipg[100];
static int portg;
extern char *cam_indexg;
extern int frame_skip_g;
extern float frame_time_g;
extern int paramScale;
extern int throwRepeat;
extern int fmCacheSize;
extern int ftpCacheSize;
extern char* srcID;
extern char* softwareVersion;
extern char* softwareVersionCreateT;
extern float persontop;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
list* pictureList = NULL;
extern int ftp(char *, int, char file[]);
void modiftFtp(const char *ip, const char *name, const char *pwd, const char *path);
void destroy();
void setupSocket(char *server, int port, char *robotID, float thresh);
int modifyIP(cJSON *pSub );
extern int printfFtp(char a[], int s);
extern int againConnect();
extern void modifyFtp(const char *ip, const char *name, const char *pwd, const char *path);
//struct timeval tv;
//struct timeval tvBegin;
double time_begin;
long time_begin_server;
void sendData(char*, int, float);
extern double get_wall_time();
int send_en(int fd,char* content,int len_char,int param);
int recv_en(int fd,char* content,int len,int param);
int getASE(int fd);
void setupSocketTimeOut(const char* timeChar, int len)
{
//	int i = 0;
//	char setT[CHAR20] = {0};
	float value = atof(timeChar);
	socket_sec = (int)value;
	socket_usec = (value - socket_sec) * 1000;
	printf("socket%d set sec=%d,usec=%d\n", len, socket_sec, socket_usec);
}

char *request(char param, char *url)
{
	cJSON *pJsonRoot = NULL;
	pJsonRoot = cJSON_CreateObject();
	if (NULL == pJsonRoot)
	{
		//error happend here
		return NULL;
	}
	cJSON_AddStringToObject(pJsonRoot, "version", "1.0.0");
	cJSON_AddStringToObject(pJsonRoot, "messageid", "123456");
	cJSON_AddStringToObject(pJsonRoot, "mode", "request");
	cJSON_AddStringToObject(pJsonRoot, "robotid", robotIDg);
	cJSON_AddStringToObject(pJsonRoot, "targetID", "4");
	cJSON_AddStringToObject(pJsonRoot, "sourceID", srcID);
	switch (param)
	{
	case 'a':
		cJSON_AddStringToObject(pJsonRoot, "orderType", "pdaHeart" );
		break;
	case 'b':
		cJSON_AddStringToObject(pJsonRoot, "orderType", "personDetect");
		break;
	case 'l':
		cJSON_AddStringToObject(pJsonRoot, "orderType", "loginMainServer");
		cJSON * param = cJSON_CreateObject();
		cJSON_AddStringToObject(param, "softwareVersion", softwareVersion);
		cJSON_AddStringToObject(param, "softwareVersionCreateT", softwareVersionCreateT);
		cJSON_AddItemToObject(pJsonRoot, "param", param);
		break;
	}
	if (NULL != url)
	{
		cJSON_AddStringToObject(pJsonRoot, "param", url);
	}
	char *p = cJSON_Print(pJsonRoot);
	cJSON_Delete(pJsonRoot);
	return p;
}
void write_to_cfg()
{
	char buf[MAXLINE];
	int thresh = 100 * threshg;
	FILE *pFile = fopen("robot.cfg", "w");
	int top = 100 * persontop;
	printf("# # # write persontop %f\n",top);
	sprintf(buf, "robot_id=%s\nserver_ip =%s\nserver_port=%d\nftp_thresh=%d\ncamera=%s\nframe_skip=%d\nsocket_time_out=%.3f\nframe_time=%.1f\ncompressibility=%d\nthrowRepeat=%d\nsrcID=%s\nfmCacheSize=%d\nftpCacheSize=%d\nsoftwareVersion=%s\nsoftwareVersionCreateT=%s\npersontop=%d\n", robotIDg, ipg, portg, thresh, cam_indexg, frame_skip_g, socket_sec + 0.001 * socket_usec, frame_time_g, paramScale, throwRepeat, srcID, fmCacheSize, ftpCacheSize, softwareVersion, softwareVersionCreateT, top);
	fwrite (buf, 1, strnlen(buf, MAXLINE), pFile);
	printf("write server information:%s", buf);
	printfFtp(buf, MAXLINE);
	fwrite (buf, 1, strnlen(buf, MAXLINE), pFile);
	printf("write Ftp information:%s", buf);
	fclose(pFile);
}
int modifyIP(cJSON *pSub )
{
	if (NULL == pSub)
		return -1;
	cJSON *pSubSubIp = cJSON_GetObjectItem(pSub, "serverip");
	cJSON *pSubSubPort = cJSON_GetObjectItem(pSub, "serverport");
	cJSON *pSubSubFtpip = cJSON_GetObjectItem(pSub, "ftpip");
	cJSON *pSubSubPortFtpuser = cJSON_GetObjectItem(pSub, "ftpusr");
	cJSON *pSubSubPortFtppwd = cJSON_GetObjectItem(pSub, "ftppwd");
	cJSON *pSubSubPortftpdpath = cJSON_GetObjectItem(pSub, "ftppdpath");
	destroy();
	int port = atoi(pSubSubPort->valuestring);
	setupSocket(pSubSubIp->valuestring, port, robotIDg, threshg);
	modifyFtp(pSubSubFtpip->valuestring, pSubSubPortFtpuser->valuestring, pSubSubPortFtppwd->valuestring, pSubSubPortftpdpath->valuestring);
	return 1;
}
char *getParam(char *pMsg)
{
	if (NULL == pMsg)
	{
		printf("jsonpMsg is NULL\n");
		return NULL;
	}
	cJSON *pJson = cJSON_Parse(pMsg);
	if (NULL == pJson)
	{
		printf("json parse fail\n");
		return NULL;
	}
	cJSON *pSub = cJSON_GetObjectItem(pJson, "param");
	cJSON *orderType = cJSON_GetObjectItem(pJson, "orderType");
	if (NULL == orderType)
		return NULL;
	if (0 == strncmp(orderType->valuestring, "pdParam", 7))
	{
		cJSON *pSubSub = cJSON_GetObjectItem(pSub, "thresh");
		char *thrChar = pSubSub->valuestring;
		if (NULL == thrChar)
		{
			printf("recv data thresh error");
		}
		else if ('0' == thrChar[0])
		{
			threshg = atof(thrChar);
		}
		else
		{
			int th = atoi(pSubSub->valuestring);
			threshg = 0.01 * th;
		}
		printf("modifty param:%f\n", threshg);
		write_to_cfg();
	}
	else if (0 == strncmp(orderType->valuestring, "pDebug", 6))
	{
		cJSON *pSubSub = cJSON_GetObjectItem(pSub, "thresh");
		char *thrChar = pSubSub->valuestring;
		if (NULL == thrChar)
		{
			printf("recv data thresh error");
		}
		else if ('0' == thrChar[0])
		{
			threshg = atof(thrChar);
		}
		else
		{
			int th = atoi(pSubSub->valuestring);
			threshg = 0.01 * th;
		}
		pSubSub = cJSON_GetObjectItem(pSub, "paramScale");
		paramScale = atoi(pSubSub->valuestring);
		pSubSub = cJSON_GetObjectItem(pSub, "throwRepeat");
		throwRepeat = atoi(pSubSub->valuestring);
		pSubSub = cJSON_GetObjectItem(pSub, "fmCacheSize");
		fmCacheSize = atoi(pSubSub->valuestring);
		pSubSub = cJSON_GetObjectItem(pSub, "ftpCacheSize");
		ftpCacheSize = atoi(pSubSub->valuestring);
		pSubSub = cJSON_GetObjectItem(pSub, "filterFmSize");
		frame_skip_g = atoi(pSubSub->valuestring);
		printf("modify param information@@@\n");
		write_to_cfg();
	}
	else if (0 == strncmp(orderType->valuestring, "serverAD", 8))
	{
		modifyIP(pSub);
		printf("modifyIP and_ftp\n");
		write_to_cfg();
	}
	else if (0 == strncmp(orderType->valuestring, "loginMainServer", 15))
	{
		cJSON *pSubSub = cJSON_GetObjectItem(pSub, "time");
		time_begin_server = atol(pSubSub->valuestring);
		//sscanf(pSubSub->valuestring,"%ld",time_begin_server);
		time_begin = get_wall_time();
		printf("reve time:%lf\n", time_begin);
//		struct tm tptr;
		//  struct timeval tv;

		//20150427153618
		/* char timeCh[5]={0};
		 strncpy(timeCh,&pSubSub->valuestring[0],4);
		 int timeInt=atoi(timeCh);
		tptr.tm_year = timeInt - 1900;

		  strncpy(timeCh,&pSubSub->valuestring[4],2);
		              timeCh[2]=0;
		              timeInt=atoi(timeCh);
		  tptr.tm_mon = timeInt - 1;

		  strncpy(timeCh,&pSubSub->valuestring[6],2);
		              timeCh[2]=0;
		              timeInt=atoi(timeCh);
		  tptr.tm_mday = timeInt;

		  strncpy(timeCh,&pSubSub->valuestring[8],2);
		              timeCh[2]=0;
		              timeInt=atoi(timeCh);
		              tptr.tm_hour = timeInt;

		  strncpy(timeCh,&pSubSub->valuestring[10],2);
		              timeCh[2]=0;
		              timeInt=atoi(timeCh);
		  tptr.tm_min = timeInt;

		  strncpy(timeCh,&pSubSub->valuestring[12],2);
		              timeCh[2]=0;
		              timeInt=atoi(timeCh);
		  tptr.tm_sec = timeInt;

		  printf("set year(%d,%d,%d,%d,%d,%d)i\n",tptr.tm_year+1900,tptr.tm_mon+1,tptr.tm_mday,tptr.tm_hour,tptr.tm_min,tptr.tm_sec);
		  tv.tv_sec = mktime(&tptr);
		  tv.tv_usec = 0;
		//    settimeofday(&tv, NULL);
		              gettimeofday(&tvBegin,NULL);
		//    strncpy(servertime, pSubSub->valuestring, 30);*/
		//  servertime = pSubSub->valuestring;
	}
	//cJSON_AddNumberToObject(pJson,"statusCode",1000);
	cJSON_AddStringToObject(pJson, "statusCode", "1000");
	char *p = cJSON_Print(pJson);
	cJSON_Delete(pJson);
	return p;
}
void reciveData()
{
	if (sockfd < 0)
	{
		printf("recive fd failed");
		return;
	}
	// printf("sj1 finish\n");
	int rec_len = -1;
	int sum_len = 0;
	char buf[MAXLINE] = {0};
	//char *message[10];
	do
	{
		rec_len = recv_en(sockfd, &buf[sum_len], MAXLINE, 0);
		if (rec_len <= 0)
		{
			printf("recv error");
			printf("rec_len=%d,connect has bean close , reconnect... ...\n", rec_len);
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
			destroy();
			setupSocket(ipg, portg, robotIDg, threshg);
			return;
		}
		printf("recv data:%s\n,len:%d\n", &buf[sum_len], rec_len);
		sum_len += rec_len;
	}
	while (rec_len == MAXLINE);
	int match = 0;
//  int begin=0;
	for (rec_len = 0; rec_len < sum_len; rec_len++)
	{
		switch (buf[rec_len])
		{
		case '{':
			match++;
			if (1 == match)
			{
				printf("recvData:%s", &buf[rec_len]);
				char *ret = getParam(&buf[rec_len]);
				if (NULL != ret)
					free(ret);
			}
			break;
		case '}':
			match--;
			break;
		}
	}
}

void setupSocket(char *server, int port, char *robotID, float thresh)
{
	int static socketAgainCount = 0;
	printf("setupSocket %s,%d,%s,%.2f,%d\n", server, port, robotID, thresh, socketAgainCount);
	if (socketAgainCount > 10)
	{
		//exit(0);
	}
	socketAgainCount++;
	robotIDg = robotID;
//	int rec_len;
//	char    buf[MAXLINE];
	struct sockaddr_in    servaddr;
	threshg = thresh;
	strcpy(ipg, server);
	portg = port;
	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		return;
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if ( inet_pton(AF_INET, server, &servaddr.sin_addr) <= 0)
	{
		printf("inet_pton error for %s\n", server);
		destroy();
		return;
		// exit(0);
	}
	if ( connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
		//  exit(0); a
		destroy();
		return;
	}
	//struct timeval timeout = {0, 20};
	//设置发送超时
//setsockopt(sockfd，SOL_SOCKET,SO_SNDTIMEO，(char *)&timeout,sizeof(struct timeval));
//设置接收超时
//setsockopt(sockfd，SOL_SOCKET,SO_RCVTIMEO，(char *)&timeout,sizeof(struct timeval));
	getASE(sockfd);


	printf("send msg to server: \n");
	//fgets(sendline, 4096, stdin);
	char *sendjson = request('l', NULL);
	int len = strlen(sendjson);
	char mess[/*messlen*/1000];
	long crc = Crc32((unsigned char *)sendjson, len);
	sprintf(mess, "ContentLength:%d\r\n%s\r\nCRC32Verify:%ld\\", len, sendjson, crc);
	// printf("sj finish\n");
	printf("send message:%s,%d\n", mess, (int)strlen(mess));
	int sendLen = send_en(sockfd, mess, strlen(mess), 0) ;
	if (  sendLen != (int)strlen(mess))
	{
		printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
	}
	if (NULL != sendjson)
	{
		free(sendjson);
	}
	else
	{
		printf("json create error\n");
	}
	reciveData();
	printf("socketsetup finish\n");
	pictureList = make_list();
	//pthread_cleanup_push(cleanup_handler, p);
	while (1)
	{
		printf("lock\n");
		pthread_mutex_lock(&mtx);
		//这个mutex主要是用来保证pthread_cond_wait的并发性
		while (pictureList->size == 0)
		{
			printf("pictureList size is 0,so wait detect person into pictureList\n");
			pthread_cond_wait(&cond, &mtx);
			printf("get a picture in pictureList,so notify , get picture ready!picture size=%d\n", pictureList->size);
		}
		printf("get picture,picturelist size=%d\n", pictureList->size);
		//CvMat* mat = (CvMat*)list_pop_front(pictureList);
		IplImage* mat = (IplImage*)list_pop_front(pictureList);

		time_t timep;
		struct tm *p;
		struct timeval tval;
		gettimeofday(&tval, NULL);
		p = localtime(&timep); /*取得当地时间*/
		char fileOut[50];
		sprintf(fileOut, "FTP-%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d%ld.jpg", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, tval.tv_usec);
		char path[100];
		strcpy(path, "/home/nvidia/imagetest/");
		strcat(path, fileOut);
		//cvSaveImage(path,mat,0);
		pthread_mutex_unlock(&mtx);             //临界区数据操作完毕，释放互斥锁
		if (NULL != mat)
		{
			printf("ready send data\n");
			int param[2];
			param[0] = CV_IMWRITE_JPEG_QUALITY;
			param[1] = 95;//paramScale;default(95) 0-100
			double stt = get_wall_time();
			CvMat* matEn = cvEncodeImage(".jpg", mat, param);
			// sendData((char *)mat->data.ptr, mat->rows * mat->cols, 1);
			sendData((char *)matEn->data.ptr, matEn->rows * matEn->cols, 1);
			cvReleaseMat(&matEn);
			cvReleaseImage(&mat);
		}
		else
		{
			printf("ready send heartbeat\n");
			sendData((char *)NULL, 0, 0);
		}

	}

}
void sendData(char *data, int size, float thresh)
{
	printf("socket send,begin!\n");
	if (-1 == sockfd)
	{
		//used stop socket for debug
		if (portg < 10)
		{
			printf("port is smaller than 10,so stop socket debug");
			return;
		}
		printf("socketfd is -1,begin link\n");
		destroy();
		setupSocket(ipg, portg, robotIDg, threshg);
		return;
	}
	static unsigned long long index = 0;
	char retData = 'a';
	char *url = NULL;
	char fileOut[100] = {0};
	printf("yolo check thresh=%f,", thresh);
	if (NULL != data && size > 0 && thresh >= threshg)
	{
		printf("ftp_thresh=%f\n", threshg);
		if (ftp(data, size, fileOut) == 0)
		{
			/* char *prex="/picture/persondetect/";
			 char name[100]={0};
			   strncpy(name,&fileOut[strlen(prex)],50);
			  printf("saveFile:%s",name);
			FILE *fp;
			                     if(fp=fopen(name,"wb"))
			     {
			         fwrite(data,1,size,fp);
			         puts("打开文件成功");
			     }
			     else
			         puts("打开文件成败");
			     fclose(fp); */
			url = &fileOut[0];
			retData = 'b';
		}
		else
		{
			printf("ftp is error,again connect\n");
			againConnect();
		}
	}
	char *sj = request(retData, url);
	int len = strlen(sj);
	char mess[1000];
	long crc = Crc32((unsigned char *)sj, len);
	sprintf(mess, "ContentLength:%d\r\n%s\r\nCRC32Verify:%ld\\", len, sj, crc);
	if (NULL != sj)
		free(sj);
	if ( send_en(sockfd, mess, strlen(mess), MSG_NOSIGNAL) <= 0)
	{
		printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
		printf("begin again link to server(%s,%d,%s,%f)\n", ipg, portg, robotIDg, threshg);
		//again setup
		destroy();
		setupSocket(ipg, portg, robotIDg, threshg);
		return;
	}
	/*  if (-1 == sockfd)
	{
	    printf("socket again link , but sockfd is still error\n");
	    return;
	}*/
	fd_set fdR;
	FD_ZERO(&fdR);
	FD_SET(sockfd, &fdR);
	struct timeval timeout;
	timeout.tv_usec = socket_usec;
	timeout.tv_sec = socket_sec;
	int selectRet = select(sockfd + 1, &fdR, NULL, NULL, &timeout);
	switch (selectRet)
	{
	case -1:
		printf("select -1\n");
		printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
		break;
	case 0:
		printf("select is not recv new message\n");
		break;
	default:
		if (FD_ISSET(sockfd, &fdR))
		{
			printf("select is recv,selectRet=%d\n", selectRet);
			reciveData();
		}

	}
	printf("socket finsh num:%lld\n", index++);
}

void destroy()
{
	if (close(sockfd) < 0)
	{
		printf("close error\n");
	}
	sockfd = -1;
}
