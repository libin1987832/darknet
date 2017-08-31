#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#define MAXLINE 4096
#define CHAR20 20
#include <cJSON.h>
#include "verifyutil.h"
int sockfd;
int socket_sec = 0;
int socket_usec = 10;
static char *robotIDg = (char *) "";
static float threshg = 0;
static char ipg[100];
static int portg;
extern char *cam_indexg;
extern int frame_skip_g;
extern float frame_time_g;
extern int ftp(char *, int, char file[]);
void modiftFtp(const char *ip, const char *name, const char *pwd, const char *path);
void destroy();
void setupSocket(char *server, int port, char *robotID, float thresh);
int modifyIP(cJSON *pSub );
extern int printfFtp(char a[], int s);
extern int againConnect();
extern void modifyFtp(const char *ip, const char *name, const char *pwd, const char *path);
void setupSocketTimeOut(const char *timeChar, int len)
{
    //  int i = 0;
    //  char setT[CHAR20] = {0};
    float value = atof(timeChar);
    socket_sec = (int)value;
    socket_usec = (value - socket_sec) * 1000;
    printf("socket%d set sec=%d,usec=%d\n",len, socket_sec, socket_usec);
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
    cJSON_AddStringToObject(pJsonRoot, "sourceID", "6");
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

    sprintf(buf, "robot_id=%s\nserver_ip =%s\nserver_port=%d\nftp_thresh=%d\ncamera=%s\nframe_skip=%d\nsocket_time_out=%.3f\nframe_time=%.1f\n", robotIDg, ipg, portg, thresh, cam_indexg, frame_skip_g, socket_sec + 0.001 * socket_usec, frame_time_g);
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
    else if (0 == strncmp(orderType->valuestring, "serverAD", 8))
    {
        modifyIP(pSub);
        printf("modifyIP and_ftp\n");
        write_to_cfg();
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
        rec_len = recv(sockfd, &buf[sum_len], MAXLINE, 0);
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
        exit(0);
    }
    socketAgainCount++;
    robotIDg = robotID;
    //  int rec_len;
    //  char    buf[MAXLINE];
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
    printf("send msg to server: \n");
    //fgets(sendline, 4096, stdin);
    char *sendjson = request('l', NULL);
    int len = strlen(sendjson);
    char mess[/*messlen*/1000];
    long crc = Crc32((unsigned char *)sendjson, len);
    sprintf(mess, "ContentLength:%d\r\n%s\r\nCRC32Verify:%ld\\", len, sendjson, crc);
    // printf("sj finish\n");
    printf("send message:%s,%d\n", mess, (int)strlen(mess));
    int sendLen = send(sockfd, mess, strlen(mess), 0) ;
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
}
void sendData(char *data, int size, float thresh)
{
    printf("socket send,begin!\n");
    if (-1 == sockfd)
    {
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
    if ( send(sockfd, mess, strlen(mess), MSG_NOSIGNAL) <= 0)
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
