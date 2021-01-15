#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUF_SIZE 64

class UtilTimer;

//用户结构体
struct ClientData
{
				sockaddr_in address;
				int sockfd;
				char buf[BUF_SIZE];
				UtilTimer *timer;
};

//定时器类
class UtilTimer
{
				public:
								UtilTimer():prev(nullptr), next(nullptr)
								{}

								time_t expire;//超时时间
								void (*cbFunc)(ClientData*);//回调函数
								ClientData* userData;
								UtilTimer *prev;//前一个定时器
								UtilTimer *next;//下一个定时器
};

//定时器链表,升序,双向链表
class TimerList
{
				public:
								TimerList():head(nullptr), tail(nullptr)
								{}

								virtual ~TimerList()
								{
												UtilTimer *tmp=head;
												while(head)
												{
																head=tmp->next;
																delete tmp;
																tmp=head;
												}
								}

								//将定时器timer加入链表
								void addTimer(UtilTimer *timer)
								{
												if(!timer)
																return;
												if(!head)
												{
																head=tail=timer;
																return;
												}
												if(timer->expire<head->expire)
												{
																timer->next=head;
																head->prev=timer;
																head=timer;
																return;
												}
												addTimer(timer, head);
								}

								//调整定时器位置
								void adjustTimer(UtilTimer *timer)
								{
												if(!timer)
																return;
												UtilTimer *tmp=timer->next;
												if(!tmp || timer->expire<=tmp->expire)
																return;
												if(timer==head)
												{
																head=head->next;
																head->prev=nullptr;
																timer->next=nullptr;
																addTimer(timer, head);
												}
												else
												{
																timer->next->prev=timer->prev;
																timer->prev->next=timer->next;
																addTimer(timer, timer->next);
												}
								}

								//删除定时器
								void delTimer(UtilTimer *timer)
								{
												if(!timer)
																return;
												if(timer==head && timer==tail)
												{
																head=nullptr;
																tail==nullptr;
																delete timer;
																return;
												}
												if(timer==head)
												{
																head=timer->next;
																head->prev=nullptr;
																delete timer;
																return;
												}
												if(timer==tail)
												{
																tail=timer->prev;
																tail->next=nullptr;
																delete timer;
																return;
												}
												timer->next->prev=timer->prev;
												timer->prev->next=timer->next;
												delete timer;
								}

								//执行链表上的任务
								void tick()
								{
												printf("timer tick\n");
												time_t currTime=time(NULL);
												UtilTimer *currTimer=head;
												while(currTimer && currTimer->expire<=currTime)
												{
																currTimer->cbFunc(currTimer->userData);
																head=currTimer->next;
																if(head)
																				head->prev=nullptr;
																delete currTimer;
																currTimer=head;
												}
								}
				private:
								//将定时器加入某个节点后面的链表
								void addTimer(UtilTimer *targetTimer, UtilTimer *begTimer)
								{
												UtilTimer *currTimer=begTimer;
												while(currTimer && currTimer->expire < targetTimer->expire)
												{
																currTimer=currTimer->next;
												}
												if(currTimer)
												{
																currTimer->prev->next=targetTimer;
																targetTimer->prev=currTimer->prev;

																targetTimer->next=currTimer;
																currTimer->prev=targetTimer;
												}
												else
												{
																tail->next=targetTimer;
																targetTimer->prev=tail;
																targetTimer->next=nullptr;
																tail=targetTimer;
												}
								}
				private:
								UtilTimer *head;
								UtilTimer *tail;
};

#endif
