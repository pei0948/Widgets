#ifndef HTTPCONN_H
#define HTTPCONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "../chapter14/Locker.hpp"

const char* ok200title="OK";
const char* error400title="Bad Request";
const char* error400form="Your request has the bad syntax or is inherently impossible to satisfy.\n";
const char* error403title="Forbidden";
const char* error403form="You do not have permission to get file from this server.\n";
const char* error404title="Not Found";
const char* error404form="The requested file was not found on this server\n";
const char* error500title="Internal Error";
const char* error500form="There was an unusual problem serving the requested file\n";

const char* docRoot="/var/www/html";

int setNonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption|O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

void addFd(int epollFd, int fd, bool oneShot)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN|EPOLLET|EPOLLRDHUP;
				if(oneShot)
								event.events|=EPOLLONESHOT;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
				setNonblock(fd);
}

void removeFd(int epollFd, int fd)
{
				epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, 0);
				close(fd);
}

//更改fd监听事件
void modFd(int epollFd, int fd, int ev)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
				epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event);
}

class HttpConn
{
				public:
								static const int FILENAME_LEN=200;//请求文件名的最大长度
								static const int READ_BUF_SIZE=2048;//读缓冲区大小
								static const int WRITE_BUF_SIZE=1024;//写缓冲区大小

								enum METHOD{GET=0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH}; //http请求方式
								enum CHECK_STAT{CHECK_STAT_REQUESTLINE=0, CHECK_STAT_HEADER, CHECK_STAT_CONTENT};//解析客户请求,主状态机所处状态
								enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD_REQUEST,
																NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
																INTERNAL_ERROR, CLOSED_CONNECTION};//http请求处理结果
								enum LINE_STAT{LINE_OK=0, LINE_BAD, LINE_OPEN};//行读取状态

				public:
								HttpConn(){};
								virtual ~HttpConn(){};

								void init(int sockFd, const sockaddr_in &addr);//初始化新接受的连接
								void closeConn(bool realClose=true);//关闭连接
								void process();//处理客户请求
								bool read();//非阻塞读操作
								bool write();//非阻塞写操作

				private:
								void init();//初始化连接
								HTTP_CODE processRead();//解析HTTP请求
								bool processWrite(HTTP_CODE res);//填充HTTP应答


								//由processRead调用,分析http请求
								HTTP_CODE parseRequestLine(char* text);
								HTTP_CODE parseHeaders(char* text);
								HTTP_CODE parseContent(char* text);
								HTTP_CODE doRequest();
								char* getLine(){return mReadBuf+mStartLine;}
								LINE_STAT parseLine();

								//由processWrite调用,填充http应答
								void unmap();
								bool addResponse(const char* format, ...);
								bool addContent(const char* content);
								bool addStatLine(int stat, const char* title);
								bool addHeaders(int contentLength);
								bool addContentLen(int contentLength);
								bool addLinger();
								bool addBlankLine();


				public:
								static int mEpollFd;//所有socket上的事件都被注册到mEpollFd上
								static int mUserCnt;//统计用户数量

				private:
								int mSockFd;//连接socket
								sockaddr_in mAddr;//客户地址

								char mReadBuf[READ_BUF_SIZE];//读缓冲区
								char mReadIdx;//读入数据的下一个位置
								int mCheckIdx;//指向读缓冲区中正在解析的位置
								int mStartLine;//正在解析的行的起始位置
								
								char mWriteBuf[WRITE_BUF_SIZE];//写缓冲区
								int mWriteIdx;//写缓冲区中,待发送的字节数

								CHECK_STAT mCheckStat;//主状态机所处的状态

								METHOD mMethod;//请求方法

								char mRealFile[FILENAME_LEN];//客户请求文件的完整路径
								char* mUrl;//客户请求的目标文件名
								char* mVersion;//HTTP协议版本号,此处仅支持HTTP1.1
								char* mHost;//主机名
								int mContentLen;//HTTP请求消息体的长度
								bool mLinger;//HTTP请求是否保持连接

								char* mFileAddress;//客户请求的目标文件被mmap映射到内存中的起始位置

								struct stat mFileStat;//目标文件的状态
								
								struct iovec mIv[2];
								int mIvCnt;//被写内存块的数量
};

int HttpConn::mEpollFd=-1;
int HttpConn::mUserCnt=0;

void HttpConn::init(int sockFd, const sockaddr_in &addr)
{
				mSockFd=sockFd;
				mAddr=addr;
				/*
				 * 避免TIME_WAIT(仅用于调试)
				 *int reuse=1;
				 *setsockopt(mSockFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
				 * */
				addFd(mEpollFd, sockFd, true);
				++mUserCnt;
				
				init();
}

void HttpConn::init()
{
				mCheckStat=CHECK_STAT_REQUESTLINE;
				mLinger=false;

				mMethod=GET;
				mUrl=nullptr;
				mVersion=nullptr;
				mHost=nullptr;
				mContentLen=0;
				mStartLine=0;
				mCheckIdx=0;
				mReadIdx=0;
				mWriteIdx=0;

				memset(mReadBuf, '\0', READ_BUF_SIZE);
				memset(mWriteBuf, '\0', WRITE_BUF_SIZE);
				memset(mRealFile, '\0', FILENAME_LEN);
}

void HttpConn::closeConn(bool realClose)
{
				if(realClose && mSockFd!=-1)
				{
								removeFd(mEpollFd, mSockFd);
								mSockFd=-1;
								--mUserCnt;
				}
}

//划分行
HttpConn::LINE_STAT HttpConn::parseLine()
{
				char tmp;
				for(; mCheckIdx<mReadIdx; ++mCheckIdx)
				{
								tmp=mReadBuf[mCheckIdx];
								if(tmp=='\r')
								{
												if(mCheckIdx+1==mReadIdx)
												{
																return LINE_OPEN;
												}
												else if(mReadBuf[mCheckIdx+1]=='\n')
												{
																mReadBuf[mCheckIdx]='\0';
																++mCheckIdx;
																mReadBuf[mCheckIdx]='\0';
																++mCheckIdx;
																return LINE_OK;
												}
												return LINE_BAD;
								}
								else if(tmp=='\n')
								{
												if(mCheckIdx>1 && mReadBuf[mCheckIdx-1]=='\r')
												{
																mReadBuf[mCheckIdx-1]='\0';
																mReadBuf[mCheckIdx]='\0';
																++mCheckIdx;
																return LINE_OK;
												}
								}
				}
				return LINE_OPEN;
}

//读取客户数据，直到无数据可读或者对方关闭连接
bool HttpConn::read()
{
				if(mReadIdx>=READ_BUF_SIZE)
								return false;
				int byteCnt=0;
				while(true)
				{
								byteCnt=recv(mSockFd, mReadBuf+mReadIdx, READ_BUF_SIZE-1-mReadIdx, 0);
								if(byteCnt==-1)
								{
												if(errno==EAGAIN || errno==EWOULDBLOCK)
																break;
												return false;
								}
								else if(byteCnt==0)
								{
												return false;
								}
								mReadIdx+=byteCnt;
				}
				return true;
}

//解析Http请求行，获得请求方法，目标URL，以及Http版本号
HttpConn::HTTP_CODE HttpConn::parseRequestLine(char* text)
{
				mUrl=strpbrk(text, " \t");
				if(mUrl==nullptr)
								return BAD_REQUEST;
				*mUrl='\0';
				++mUrl;

				char* method=text;
				if(strcasecmp(method, "GET")==0)
								mMethod=GET;
				else
								return BAD_REQUEST;
				mUrl+=strspn(mUrl, " \t");
				mVersion=strpbrk(mUrl, " \t");
				if(mVersion==nullptr)
								return BAD_REQUEST;
				*mVersion='\0';
				++mVersion;
				mVersion+=strspn(mVersion, " \t");
				if(strcasecmp(mVersion, "HTTP/1.1")!=0)
								return BAD_REQUEST;
				if(strncasecmp(mUrl, "http://", 7)==0)
				{
								mUrl+=7;
								mUrl=strchr(mUrl, '/');
				}
				if(mUrl==nullptr || mUrl[0]!='/')
								return BAD_REQUEST;
				mCheckStat=CHECK_STAT_HEADER;
				return NO_REQUEST;
}

//解析Http请求的一个头部信息
HttpConn::HTTP_CODE HttpConn::parseHeaders(char* text)
{
				//遇到空行，表示头部字段解析完毕
				if(text[0]=='\0')
				{
								//如果HTTP请求有消息体，则还需要读取mContentLen
								if(mContentLen!=0)
								{
												mCheckStat=CHECK_STAT_CONTENT;
												return NO_REQUEST;
								}
								return GET_REQUEST;
				}
				else if(strncasecmp(text, "Connection:", 11)==0)//处理Connection头部字段
				{
								text+=11;
								text+=strspn(text, " \t");
								if(strcasecmp(text, "keep-alive")==0)
												mLinger=true;
				}
				else if(strncasecmp(text, "Content-Length:", 15)==0)//处理Content-Length头部字段
				{
								text+=15;
								text+=strspn(text, " \t");
								mContentLen=atoi(text);
				}
				else if(strncasecmp(text, "Host:", 5)==0)//处理Host头部字段
				{
								text+=5;
								text+=strspn(text, " \t");
								mHost=text;
				}
				else
				{
								printf("oop!unknow header %s\n", text);
				}
				return NO_REQUEST;
}

//没有真正解析HTTP请求消息体，只判断它是否被完整地读入了
HttpConn::HTTP_CODE HttpConn::parseContent(char* text)
{
				if(mReadIdx>=(mContentLen+mCheckIdx))
				{
								text[mContentLen]='\0';//如果说text是从消息体头部开始的，那就合理了
								return GET_REQUEST;
				}
				return NO_REQUEST;
}

//主状态机
HttpConn::HTTP_CODE HttpConn::processRead()
{
				LINE_STAT lineStat=LINE_OK;
				HTTP_CODE res=NO_REQUEST;
				char* text=nullptr;
				while((mCheckStat==CHECK_STAT_CONTENT && lineStat==LINE_OK)
												||((lineStat==parseLine())==LINE_OK))
				{
								text=getLine();
								mStartLine=mCheckIdx;
								printf("got 1 http line: %s\n", text);

								switch(mCheckStat)
								{
												case CHECK_STAT_REQUESTLINE:
																res=parseRequestLine(text);
																if(res==BAD_REQUEST)
																				return BAD_REQUEST;
																break;
												case CHECK_STAT_HEADER:
																res=parseHeaders(text);
																if(res==BAD_REQUEST)
																				return BAD_REQUEST;
																else if(res==GET_REQUEST)
																				return doRequest();
																break;
												case CHECK_STAT_CONTENT:
																res=parseContent(text);
																if(res==GET_REQUEST)
																				return doRequest();
																lineStat=LINE_OPEN;
																break;
												default:
																return INTERNAL_ERROR;
								}
				}
				return NO_REQUEST;
}

//处理http请求
HttpConn::HTTP_CODE HttpConn::doRequest()
{
				strcpy(mRealFile, docRoot);
				int len=strlen(docRoot);
				strncpy(mRealFile+len, mUrl, FILENAME_LEN-len-1);
				if(stat(mRealFile, &mFileStat)<0)
								return NO_RESOURCE;
				if(!(mFileStat.st_mode & S_IROTH))
								return FORBIDDEN_REQUEST;
				if(S_ISDIR(mFileStat.st_mode))
								return BAD_REQUEST;
				
				int fd=open(mRealFile, O_RDONLY);
				mFileAddress=(char*)mmap(0, mFileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
				close(fd);

				return FILE_REQUEST;
}

//对内存映射区执行munmap操作
void HttpConn::unmap()
{
				if(mFileAddress)
				{
								munmap(mFileAddress, mFileStat.st_size);
								mFileAddress=nullptr;
				}
}

//写Http响应
bool HttpConn::write()
{
				int tmp=0;
				int byteHaveSend=0;
				int byteToSend=mWriteIdx;
				if(byteToSend==0)
				{
								modFd(mEpollFd, mSockFd, EPOLLIN);
								init();
								return true;
				}
				while(1)
				{
								tmp=writev(mSockFd, mIv, mIvCnt);
								if(tmp<=-1)
								{
												if(errno==EAGAIN)
												{
																modFd(mEpollFd, mSockFd, EPOLLOUT);
																return true;
												}
												unmap();
												return false;
								}
								byteToSend-=tmp;
								byteHaveSend+=tmp;
								if(byteToSend<=byteHaveSend)//发送http响应成功,根据Http请求中的Connection字段决定是否立即关闭连接
								{
												unmap();
												if(mLinger)
												{
																init();
																modFd(mEpollFd, mSockFd, EPOLLIN);
																return true;
												}
												else
												{
																modFd(mEpollFd, mSockFd, EPOLLIN);
																return false;
												}
								}
				}
}

//往缓冲区里面写入待发送的数据
bool HttpConn::addResponse(const char* format, ...)
{
				if(mWriteIdx>=WRITE_BUF_SIZE)
								return false;
				va_list argList;
				va_start(argList, format);

				int len=vsnprintf(mWriteBuf+mWriteIdx, WRITE_BUF_SIZE-1-mWriteIdx, format, argList);
				if(len>=(WRITE_BUF_SIZE-1-mWriteIdx))
								return false;
				mWriteIdx+=len;
				va_end(argList);
				
				return true;
}

bool HttpConn::addStatLine(int stat, const char* title)
{
				return addResponse("%s %d %s\r\n", "HTTP/1.1", stat, title);
}

bool HttpConn::addHeaders(int contentLen)
{
				addContentLen(contentLen);
				addLinger();
				addBlankLine();
}

bool HttpConn::addContentLen(int contentLen)
{
				return addResponse("Content-Length: %d\r\n", contentLen);
}

bool HttpConn::addLinger()
{
				return addResponse("Connection: %s\r\n", (mLinger==true)?"keep-alive":"close");
}

bool HttpConn::addBlankLine()
{
				return addResponse("%s", "\r\n");
}

bool HttpConn::addContent(const char* content)
{
				return addResponse("%s", content);
}

//根据Http请求处理结果,决定返回客户端内容
bool HttpConn::processWrite(HTTP_CODE res)
{
				switch(res)
				{
								case INTERNAL_ERROR:
												printf("500\n");
												addStatLine(500, error500title);
												addHeaders(strlen(error500form));
												if(!addContent(error500form))
																return false;
												break;
								case BAD_REQUEST:												
												printf("400\n");
												addStatLine(400, error400title);
												addHeaders(strlen(error400form));
												if(!addContent(error400form))
																return false;
												break;
								case NO_RESOURCE:
												printf("404\n");
												addStatLine(404, error404title);
												addHeaders(strlen(error404form));
												if(!addContent(error404form))
																return false;
												break;
								case FORBIDDEN_REQUEST:
												printf("403\n");
												addStatLine(403, error403title);
												addHeaders(strlen(error403form));
												if(!addContent(error403form))
																return false;
												break;
								case FILE_REQUEST:
												printf("200\n");
												addStatLine(200, ok200title);
												if(mFileStat.st_size!=0)
												{
																addHeaders(mFileStat.st_size);
																mIv[0].iov_base=mWriteBuf;
																mIv[0].iov_len=mWriteIdx;
																mIv[1].iov_base=mFileAddress;
																mIv[1].iov_len=mFileStat.st_size;
																mIvCnt=2;
																return true;
												}
												else
												{
																const char* okString="<html><body></body></html>";
																addHeaders(strlen(okString));
																if(!addContent(okString))
																				return false;
												}
												break;
								default:
												return false;
												break;
				}
				mIv[0].iov_base=mWriteBuf;
				mIv[0].iov_len=mWriteIdx;
				mIvCnt=1;
				return true;
}

//http请求入口函数
void HttpConn::process()
{
				HTTP_CODE readRes=processRead();
				if(readRes==NO_REQUEST)
				{
								printf("NO_REQUEST\n");
								modFd(mEpollFd, mSockFd, EPOLLIN);
								return;
				}

				bool writeRes=processWrite(readRes);
				if(!writeRes)
								closeConn();
				modFd(mEpollFd, mSockFd, EPOLLOUT);
}

#endif
