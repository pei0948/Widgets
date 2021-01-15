#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define BUF_SIZE 4096

//两种状态:分析请求行,分析头部字段
enum CHECK_STAT{CHECK_STAT_REQUESTLINE=0, CHECK_STAT_HEADER};
//行读取状态:完整的行,行出错,行数据还不完整
enum LINE_STAT{LINE_OK=0, LINE_BAD, LINE_OPEN};

enum HTTP_CODE{NO_REQUEST,//请求不完整
								GET_REQUEST,//获得了一个完整的请求
								BAD_REQUEST,//客户请求有语法错误
								FORBIDDEN_REQUEST,//客户对资源没有足够的访问权限
								INTERNAL_ERROR,//服务器内部错误
								CLOSED_CONNECTION};//客户端已经关闭连接

static const char *HTTP_RESPON[]={"I get a correct result\n", "Something wrong\n"};

//解析出一行内容
LINE_STAT parseLine(char *buf, int &checkIdx, int &readIdx)
{
				char tmp;
				for(; checkIdx<readIdx; ++checkIdx)
				{
								tmp=buf[checkIdx];
								if(tmp=='\r')
								{
												if(checkIdx+1==readIdx)
												{
																return LINE_OPEN;
												}
												else if(buf[checkIdx+1]=='\n')
												{
																buf[checkIdx]='\0';
																++checkIdx;
																buf[checkIdx]='\0';
																++checkIdx;
																return LINE_OK;
												}
												return LINE_BAD;
								}
								else if(tmp=='\n')
								{
												if(checkIdx>=1 && buf[checkIdx-1]=='\r')
												{
																buf[checkIdx-1]='\0';
																buf[checkIdx]='\0';
																++checkIdx;
																return LINE_OK;
												}
												return LINE_BAD;
								}
				}
				return LINE_OPEN;
}

//分析请求行
HTTP_CODE parseRequestLine(char *tmp,  CHECK_STAT &checkStat)
{
				char *url=strpbrk(tmp, " \t");
				if(!url)
				{
								return BAD_REQUEST;
				}
				*url='\0';
				++url;
				char *method=tmp;
				if(strcasecmp(method, "GET")==0)
				{
								printf("The request method is GET\n");
				}
				else
				{
								return BAD_REQUEST;
				}
				url+=strspn(url, " \t");
				char *version=strpbrk(url, " \t");
				if(!version)
				{
								return BAD_REQUEST;
				}
				*version='\0';
				version+=strspn(version, " \t");
				if(strcasecmp(version, "HTTP/1.1")==0)
				{
								url+=7;
								url=strchr(url, '/');
				}
				if(!url || url[0]!='/')
				{
								return BAD_REQUEST;
				}
				printf("The request URL is: %s\n", url);
				checkStat=CHECK_STAT_HEADER;
				return NO_REQUEST;
}

//分析头部字段
HTTP_CODE parseHeaders(char *tmp)
{
				if(tmp[0]=='\0')
				{
								return GET_REQUEST;
				}
				else if(strncasecmp(tmp, "Host:", 5)==0)
				{
								tmp+=5;
								tmp+=strspn(tmp, " \t");
								printf("the request host is: %s\n", tmp);
				}
				else
				{
								printf("I can not handle this header\n");
				}
				return NO_REQUEST;
}

//分析HTTP请求的入口函数
HTTP_CODE parseContent(char *buf, int &checkIdx, CHECK_STAT &checkStat, int &readIdx, int &startLine)
{
				LINE_STAT lineStat=LINE_OK;
				HTTP_CODE resCode=NO_REQUEST;
				while((lineStat=parseLine(buf, checkIdx, readIdx))==LINE_OK)
				{
								char *tmp=buf+startLine;
								startLine=checkIdx;
								switch(checkStat)
								{
												case CHECK_STAT_REQUESTLINE:
																resCode=parseRequestLine(tmp, checkStat);
																if(resCode==BAD_REQUEST)
																{
																				return BAD_REQUEST;
																}
																break;
												case CHECK_STAT_HEADER:
																resCode=parseHeaders(tmp);
																if(resCode==BAD_REQUEST)
																{
																				return BAD_REQUEST;
																}
																else if(resCode==GET_REQUEST)
																{
																				return GET_REQUEST;
																}
																break;
												default:
																return INTERNAL_ERROR;
								}
								if(lineStat==LINE_OPEN)
								{
												return NO_REQUEST;
								}
								else
								{
												return BAD_REQUEST;
								}
				}
				return resCode;
}


int main(int argc, char *argv[])
{
				if(argc<=2)
				{
								printf("ip port\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				sockaddr_in clientAddr;
				bzero(&clientAddr, sizeof(clientAddr));
				socklen_t clientAddrLen=sizeof(clientAddr);

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd<0)
				{
								printf("error is %d", errno);
								close(listenSock);
								return 0;
				}

				char buf[BUF_SIZE];
				memset(buf, '\0', BUF_SIZE);
				int dataRead=0;
				int readIdx=0;//已经读取了多少字节的客户数据
				int checkIdx=0;//已经分析了多少字节的客户数据
				int startLine=0;//行在buf中的起始位置
				CHECK_STAT checkStat=CHECK_STAT_REQUESTLINE;
				while(1)
				{
								dataRead=recv(confd, buf+readIdx, BUF_SIZE-readIdx, 0);
								if(dataRead==-1)
								{
												printf("reading fails\n");
												break;
								}
								else if(dataRead==0)
								{
												printf("remote client has closed the connection\n");
												break;
								}
								readIdx+=dataRead;
								HTTP_CODE httpRes=parseContent(buf, checkIdx, checkStat, readIdx, startLine);
								if(httpRes==NO_REQUEST)
								{
												continue;
								}
								else if(httpRes==GET_REQUEST)
								{
												send(confd, HTTP_RESPON[0], strlen(HTTP_RESPON[0]), 0);
												break;
								}
								else
								{
												send(confd, HTTP_RESPON[1], strlen(HTTP_RESPON[1]), 0);
												break;
								}
				}
				close(confd);
				close(listenSock);
				return 0;

}
