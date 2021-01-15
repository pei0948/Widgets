#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUF_SIZE 1024

class TwTimer;

struct ClientData
{
				sockaddr_in address;
				int sockfd;
				char buf[BUF_SIZE];
				TwTimer *timer;
};
//时间轮定时器
class TwTimer
{
				public:
								TwTimer(int rot, int ts):next(nullptr), prev(nullptr), rotation(rot), timeSlot(ts)
								{}
				public:
								int rotation;//记录定时器在转动多少圈后生效
								int timeSlot;//第几个槽
								void (*cbFunc)(ClientData*);//定时器回调函数
								ClientData* userData;//用户数据
								TwTimer *next;
								TwTimer *prev;
};
//时间轮
class TimeWheel
{
				public:
								TimeWheel():currSlot(0)
								{
												for(int i=0; i<n; ++i)
																slots[i]=nullptr;
								}
								virtual ~TimeWheel()
								{
												for(int i=0; i<n; ++i)
												{
																TwTimer *curr=slots[i];
																while(slots[i])
																{
																				slots[i]=slots[i]->next;
																				delete curr;
																				curr=slots[i];
																}
												}
								}
								//添加定时器
								TwTimer* addTimer(int timeout)
								{
												if(timeout<=0)
																return nullptr;
												int ticks=0;
												if(timeout<slotInv)//如果超时间隔小于一个时隙,按一个时隙计算
												{
																ticks=1;
												}
												else
												{
																ticks=timeout/slotInv;
												}
												int rotation=ticks/n;
												int ts=(currSlot+(ticks%n))%n;
												TwTimer *timer=new TwTimer(rotation, ts);
												
												if(!slots[ts])
												{
																slots[ts]=timer;
												}
												else
												{
																timer->next=slots[ts];
																slots[ts]->prev=timer;
																slots[ts]=timer;
												}
												printf("add timer in %d rot %d slot\n", rotation, ts);
												return timer;
								}
								//删除定时器
								void delTimer(TwTimer *timer)
								{
												if(!timer)
																return;
												int ts=timer->timeSlot;
												if(timer==slots[ts])
												{
																slots[ts]=timer->next;
																if(slots[ts])
																				slots[ts]->prev=nullptr;
												}
												else
												{
																timer->prev->next=timer->next;
																if(timer->next)
																				timer->next->prev=timer->prev;
												}

												delete timer;
								}

								//调用函数
								void tick()
								{
												TwTimer *curr=slots[currSlot];
												printf("currslot is %d\n", currSlot);
												while(curr)
												{
																if(curr->rotation>0)
																{
																				--(curr->rotation);
																				curr=curr->next;
																}
																else
																{
																				curr->cbFunc(curr->userData);
																				if(curr==slots[currSlot])
																				{
																								slots[currSlot]=slots[currSlot]->next;
																								if(slots[currSlot])
																												slots[currSlot]->prev=nullptr;
																								delete curr;
																								curr=slots[currSlot];
																				}
																				else
																				{
																								curr->prev->next=curr->next;
																								if(curr->next)
																												curr->next->prev=curr->prev;
																								TwTimer *tmp=curr->next;
																								delete curr;
																								curr=tmp;
																				}
																}
												}
												++currSlot;
												currSlot=(currSlot%n);
								}
				private:
								static const int n=60;//时间轮上槽的数目
								static const int slotInv=1;//每隔1秒转动1次,槽间隔为1s
								TwTimer* slots[n];//时间轮的槽,每个元素指向一个定时器链表,无序链表
								int currSlot;//当前槽
};

#endif
