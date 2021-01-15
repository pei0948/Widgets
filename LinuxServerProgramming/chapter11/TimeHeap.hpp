#ifndef TIME_HEAP
#define TIME_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>
using std::exception;

#define BUF_SIZE 64

class HeapTimer;

struct ClientData
{
				sockaddr_in address;
				int sockfd;
				char buf[BUF_SIZE];
				HeapTimer *timer;
};

//定时器
class HeapTimer
{
				public:
								HeapTimer(int delay)
								{
												expire=time(NULL)+delay;
								}
				public:
								time_t expire;//超时时间
								void (*cbFunc)(ClientData *);//回调函数
								ClientData *userData;
};

//时间堆
class TimeHeap
{
				public:
								TimeHeap(int c) throw (std::exception)
								{
												capacity=c;
												size=0;
												array=new HeapTimer*[capacity];
												if(!array)
												{
																throw std::exception();
												}
												for(int i=0; i<capacity; ++i)
												{
																array[i]=nullptr;
												}
								}
								TimeHeap(HeapTimer **initArray, int s, int c) throw (std::exception)
								{
												size=s;
												capacity=c;
												if(capacity<size)
												{
																throw std::exception();
												}
												array=new HeapTimer*[capacity];
												if(!array)
												{
																throw std::exception();
												}
												for(int i=0; i<capacity; ++i)
												{
																array[i]=nullptr;
												}
												if(size!=0)
												{
																for(int i=0; i<size; ++i)
																				array[i]=initArray[i];
																for(int i=(size-1)/2; i>=0; --i)
																				percolateDown(i);
												}

								}
								virtual ~TimeHeap()
								{
												for(int i=0; i<size; ++i)
												{
																delete array[i];
												}
												delete []array;
								}

								//添加定时器
								void addTimer(HeapTimer *timer) throw (std::exception)
								{
												if(!timer)
																return;
												if(size>=capacity)
																resize();
												int hole=size;
												++size;

												int parent;
												while(hole>0)
												{
																parent=(hole-1)/2;
																if(array[parent]->expire<=timer->expire)
																				break;
																array[hole]=array[parent];
																hole=parent;
												}
												array[hole]=timer;

								}
								//删除定时器
								void delTimer(HeapTimer *timer)
								{
												if(!timer)
																return;
												timer->cbFunc=nullptr;//仅仅将目标定时器的回调函数设置为空.
								}

								HeapTimer* top() const
								{
												if(empty())
																return nullptr;
												return array[0];
								}
								//删除顶部定时器
								void popTimer()
								{
												if(empty())
																return;
												if(array[0])
												{
																delete array[0];
																--size;
																array[0]=array[size];
																percolateDown(0);
												}
								}
								//处理函数
								void tick()
								{
												HeapTimer *currTimer=array[0];
												time_t currTime=time(NULL);
												while(!empty())
												{
																if(!currTimer)
																				break;
																if(currTime<currTimer->expire)
																				break;
																if(array[0]->cbFunc)
																				array[0]->cbFunc(array[0]->userData);
																popTimer();
																currTimer=array[0];
												}
								}
								bool empty() const
								{
												return (size==0);
								}
				private:
								//下滤
								void percolateDown(int hole)
								{
												HeapTimer *tmp=array[hole];
												int child=2*hole+1;
												while(child<size)
												{
																if(child+1<size && array[child+1]->expire<array[child]->expire)
																				++child;
																if(tmp->expire<=array[child]->expire)
																				break;
																array[hole]=array[child];
																hole=child;
																child=2*hole+1;
												}
												array[child]=tmp;
								}
								void resize() throw (std::exception)
								{
												HeapTimer **tmp=new HeapTimer*[capacity<<1];
												if(!tmp)
																throw std::exception();
												capacity<<=1;
												for(int i=0; i<capacity; ++i)
																array[i]=nullptr;
												for(int i=0; i<size; ++i)
																tmp[i]=array[i];
												delete []array;
												array=tmp;
								}
				private:
								HeapTimer** array;//堆数组
								int capacity;//堆数组容量
								int size;//元素数量
};

#endif
