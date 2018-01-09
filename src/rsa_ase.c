//rsa_test.cpp  用于测试rsa和aes功能代码  
//sudo apt-get install openssl
//g++ rsa_test.c  -I/usr/include/openssl  -lpthread -lm  -lcrypto -g -o rsa_test 

#include <openssl/rsa.h>  
#include <openssl/aes.h>
#include <openssl/err.h>  
#include <openssl/pem.h>  
  
//#include <iostream>  
//#include <string>  
//#include <cstring>  
//#include <cassert>  
#include<string.h>

//using namespace std;  
char strPemFileName[1000];

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char padding_char = '=';

/*编码代码
* const unsigned char * sourcedata， 源数组
* char * base64 ，码字保存
*/
int base64_encode(const unsigned char * sourcedata, char * base64)
{
    int i=0, j=0;
    unsigned char trans_index=0;    // 索引是8位，但是高两位都为0
    const int datalength = strlen((const char*)sourcedata);
    for (; i < datalength; i += 3){
        // 每三个一组，进行编码
        // 要编码的数字的第一个
        trans_index = ((sourcedata[i] >> 2) & 0x3f);
        base64[j++] = base64char[(int)trans_index];
        // 第二个
        trans_index = ((sourcedata[i] << 4) & 0x30);
        if (i + 1 < datalength){
            trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
            base64[j++] = base64char[(int)trans_index];
        }else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            base64[j++] = padding_char;

            break;   // 超出总长度，可以直接break
        }
        // 第三个
        trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
        if (i + 2 < datalength){ // 有的话需要编码2个
            trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
            base64[j++] = base64char[(int)trans_index];

            trans_index = sourcedata[i + 2] & 0x3f;
            base64[j++] = base64char[(int)trans_index];
        }
        else{
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            break;
        }
    }

    base64[j] = '\0'; 

    return 0;
}
//读取base64字符串转为Rsa公钥
int strConvert2PublicKey( char* strPublicKey, int nPublicKeyLen,RSA* pRSAPublicKey )  
{
	//int nPublicKeyLen = strPublicKey.size();      //strPublicKey为base64编码的公钥字符串

	for(int i = 64; i < nPublicKeyLen; i+=64)
	{
		if(strPublicKey[i] != '\n')
		{
			//strPublicKey.insert(i, "\n");
			char temp[1000]={0};
			for(int j=0;j+i<nPublicKeyLen;j++)
			   {
				 temp[j]= strPublicKey[i];		 
			   }
			strPublicKey[i]='\n';
			strPublicKey[i+1]=0;
			strcat(strPublicKey, temp);
		}
		i++;
	}
	//strPublicKey.insert(0, "-----BEGIN PUBLIC KEY-----\n");
	char strPublicKeyB[1000]="-----BEGIN PUBLIC KEY-----\n";
	strcat(strPublicKeyB, strPublicKey);
	//strPublicKey.append("\n-----END PUBLIC KEY-----\n");
	strcat(strPublicKeyB, "\n-----END PUBLIC KEY-----\n");
	BIO *bio = NULL; 
	RSA *rsa = NULL; 
	char *chPublicKey = strPublicKeyB;//const_cast<char *>(strPublicKey.c_str());
	if ((bio = BIO_new_mem_buf(chPublicKey, -1)) == NULL)       //从字符串读取RSA公钥
	{     
		   // cout<<"BIO_new_mem_buf failed!"<<endl;      
	}       
	rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);   //从bio结构中得到rsa结构
	if (!rsa)
	{
		    ERR_load_crypto_strings();
		    char errBuf[512];
		    ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
		    //cout<< "load public key failed["<<errBuf<<"]"<<endl;
			BIO_free_all(bio);
	}
return 1;
}



//加密  
int EncodeRSAKeyFile( const char* strPemFileName, const char* strData ,int strDataLen,char* pEncode)  
{  
   /* if (strPemFileName.empty() || strData.empty())  
    {  
        assert(false);  
        return "";  
    }  */
    FILE* hPubKeyFile = fopen(strPemFileName, "rb");  
    if( hPubKeyFile == NULL )  
    {  
        //assert(false);  
        return "";   
    }  
    //std::string strRet;  
    RSA* pRSAPublicKey = RSA_new();  
    if(PEM_read_RSA_PUBKEY(hPubKeyFile, &pRSAPublicKey, 0, 0) == NULL)  
    {  
       // assert(false);  
        return "";  
    }  
  
    int nLen = RSA_size(pRSAPublicKey);  
    //char* pEncode = new char[nLen + 1];  
    pEncode[nLen]=0;
    int ret = RSA_public_encrypt(strDataLen, strData, (unsigned char*)pEncode, pRSAPublicKey, RSA_PKCS1_PADDING);  
   /* if (ret >= 0)  
    {  
       // strRet = std::string(pEncode, ret);  
	//return pEncode;
  	ret=nLen;
    }  */
    //delete[] pEncode;  
    RSA_free(pRSAPublicKey);  
    fclose(hPubKeyFile);  
    CRYPTO_cleanup_all_ex_data();   
    return ret;  
}  
  
//解密  
char* DecodeRSAKeyFile( const char* strPemFileName, const char* strData ,int strDataLen,char* pDecode )  
{  
   /* if (strPemFileName.empty() || strData.empty())  
    {  
        assert(false);  
        return "";  
    }  */
    FILE* hPriKeyFile = fopen(strPemFileName,"rb");  
    if( hPriKeyFile == NULL )  
    {  
      //  assert(false);  
        return "";  
    }  
    char* strRet=NULL;  
    RSA* pRSAPriKey = RSA_new();  
    if(PEM_read_RSAPrivateKey(hPriKeyFile, &pRSAPriKey, 0, 0) == NULL)  
    {  
       // assert(false);  
        return "";  
    }  
    int nLen = RSA_size(pRSAPriKey);  
    //char* pDecode = new char[nLen+1];  
    pDecode[nLen]=0;
    int ret = RSA_private_decrypt(strDataLen, (const unsigned char*)strData, (unsigned char*)pDecode, pRSAPriKey, RSA_PKCS1_PADDING);  
   /* if(ret >= 0)  
    {  
       // strRet = std::string((char*)pDecode, ret);  
	
    }  */
    //delete [] pDecode;  
    RSA_free(pRSAPriKey);  
    fclose(hPriKeyFile);  
    CRYPTO_cleanup_all_ex_data();   
    return ret;  
}  
  


//AES_加密
int EncodeAES( const char* password,int passwordLen, const char* data ,int dataLen,char* strRet)
{
    memset(strRet, 0, 1000);
    AES_KEY aes_key;
    //cout << "password_length " << password.length() <<endl;     

    if(AES_set_encrypt_key((const unsigned char*)password, passwordLen * 8, &aes_key) < 0)
    {
        //assert(false);
        return "";
    }

    //std::string strRet;
    //std::string data_bak = data;
    //unsigned int data_length = data_bak.length();
   char* data_bak[1000]={0};
   unsigned int data_length = dataLen;
    int padding = 0;
    if (/*data_bak.length()*/data_length % AES_BLOCK_SIZE > 0)
    {
       // padding =  AES_BLOCK_SIZE - data_bak.length() % AES_BLOCK_SIZE;
	padding =  AES_BLOCK_SIZE - data_length % AES_BLOCK_SIZE;
    }
    data_length += padding;
    int data_temp=data_length;
    while (padding > 0)
    {
        //data_bak += '\0';
        data_bak[data_temp]='\0';
	data_temp++;
        padding--;
    }
    for(unsigned int i = 0; i < data_length/AES_BLOCK_SIZE; i++)
    {
        //std::string str16 = data_bak.substr(i*AES_BLOCK_SIZE, AES_BLOCK_SIZE);
	char* str16[1000];
	//strcpy(str16,&data_bak[i*AES_BLOCK_SIZE],AES_BLOCK_SIZE);
	for(int j=0;j<AES_BLOCK_SIZE;j++)
	{
		str16[j]=data_bak[i*AES_BLOCK_SIZE+j];
	}
        unsigned char out[AES_BLOCK_SIZE];
        memset(out, 0, AES_BLOCK_SIZE);
        AES_encrypt((const unsigned char*)str16, out, &aes_key);
        //strRet += std::string((const char*)out, AES_BLOCK_SIZE);
	strcat(strRet,out);
    }
   // return strRet;
    return strlen(strRet);
}


//AES解密
int DecodeAES( const char* strPassword, int strPasswordLen,const char* strData ,int strDataLen,char* strRet)
{
	memset(strRet, 0, 1000);
	AES_KEY aes_key;
	if(AES_set_decrypt_key((const unsigned char*)strPassword, strPasswordLen * 8, &aes_key) < 0)
	{
		//assert(false);
		return "";
	}
	//std::string strRet;
	//for(unsigned int i = 0; i < strData.length()/AES_BLOCK_SIZE; i++)
	for(unsigned int i = 0; i < strDataLen/AES_BLOCK_SIZE; i++)
	{
		//std::string str16 = strData.substr(i*AES_BLOCK_SIZE, AES_BLOCK_SIZE);
		char* str16[1000];
		unsigned char out[AES_BLOCK_SIZE];
		memset(out, 0, AES_BLOCK_SIZE);
		AES_decrypt((const unsigned char*)str16, out, &aes_key);
		//strRet += std::string((const char*)out, AES_BLOCK_SIZE);
		strcat(strRet,out);
	}
	return strlen(strRet);
}
char chk_xrl(const char *data, int length)  
{  
  const char *buf = data;  
  char retval = 0;  
  
  while(length)  
  {  
    retval ^= *buf++;  
    --length;  
  }  
  return retval;  
}  
int getASE(int fd)
{
    if(fd<0)
        return NULL;
    char rsa_public[9]={0};
    rsa_public[0]=0x7E;
    rsa_public[1]=0x7E;
    rsa_public[2]=0x01;
    rsa_public[8]=0x0D;
    int len_en=send(fd,rsa_public,9,0);
    char rsa_rec[1000];
    recv(fd,rsa_rec,1000,0);
    int len_en1=(int)rsa_rec[3]*16*16*16;
    int len_en2=(int)rsa_rec[4]*16*16;
    int len_en3=(int)rsa_rec[5]*16;
    int len_en4=len_en1+len_en2+len_en3+rsa_rec[6];
 
    //std::string public_RSA(&rsa_rec[7],len_en4);
    char public_RSA[1000]={0};
   memcpy(public_RSA,&rsa_rec[7],len_en4);
    char baseCh[1000];
     base64_encode(public_RSA,baseCh);
    RSA rsa_p;
    strConvert2PublicKey(baseCh,len_en4,&rsa_p);
   // std::string pRSAPublicKey;
    char pRSAPublicKey[1000];
    rsa_public[2]=0x02;
    len_en=send(fd,rsa_public,9,0);
    recv(fd,rsa_rec,1000,0);
    len_en1=(int)rsa_rec[3]*16*16*16;
    len_en2=(int)rsa_rec[4]*16*16;
    len_en3=(int)rsa_rec[5]*16;
    len_en4=len_en1+len_en2+len_en3+rsa_rec[6];
    //std::string public_RSA_ASE(&rsa_rec[7],len_en4);
    char* public_RSA_ASE[1000];
    int ret = DecodeRSAKeyFile("","", 0,public_RSA_ASE);
    return ret;

}
int send_en(int fd,char* content,int len_char,int param)
{
     if(fd<0)
        return NULL;
    char send_content[1000];
    send_content[0]=0x7E;
    send_content[1]=0x7E;
    send_content[2]=0x03;

    char en_cont[1000];
    int len=EncodeAES(strPemFileName,0,content,0,en_cont);
    send_content[3+0]  = len >> 24;     
    send_content[3+1] = len >> 16;
    send_content[3+2] = len >> 8;
    send_content[3+3] = len;
    memcpy(&send_content[7],en_cont,len);
    send_content[len+7]=(char)chk_xrl(&send_content[7],len);
    send_content[len+8]=0x0D;
    return send(fd,send_content,len+9,0);
}
int recv_en(int fd,char* content,int rec_content_maxlen,int param)
{
    if(fd<0)
        return NULL;
    char rec_content[1000];
    int len=recv_en(fd,rec_content,1000,param);
    int len_c=0;
    len_c=rec_content[6];
    len_c+=rec_content[5]<<8;
    len_c+=rec_content[4]<<16;
    len_c+=rec_content[3]<<24;
    //std::string cont_str(&rec_content[7],len_c);
    char chk=(char)chk_xrl(&rec_content[7],len_c);
    if(rec_content[len+7]!=chk)
    {
        printf("recevie data sum fail\n");
        return -1;
    }
    char rec_cont[1000];
    len = DecodeAES( strPemFileName, 0,&rec_content[7] ,0,rec_cont);
    memset(content,rec_cont,len);
    return len;
}

int maintt()  
{  

    /*
    //原文  
    //AES加密，块大小必须为128位（16字节），如果不是，则要补齐，密钥长度可以选择128位、192位、256位。
    const string one = "1234567812345678";
    cout << "one: " << one << endl;  
  
    //密文（二进制数据）  
    string two = EncodeRSAKeyFile("public.pem", one);  
    cout << "two: " << two << endl;  
  
    //顺利的话，解密后的文字和原文是一致的  
    string three = DecodeRSAKeyFile("privkey.pem", two);  
    cout << "three: " << three << endl;  
    */
 

 /*   const string three = "1234567812345678"; //要加密的内容 
    const string four = "@@wufsfadfyuan@"; //要加密的内容 
	string five = EncodeAES(three, four);
    cout << "five: " << five << endl; 

	string six = DecodeAES(three, five);
    cout << "six: " << six << endl; */

    return 0;  
}
