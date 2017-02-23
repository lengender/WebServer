/*************************************************************************
	> File Name: min_heap_timer.h
	> Author: 
	> Mail: 
	> Created Time: 2017年02月22日 星期三 14时51分11秒
 ************************************************************************/

#ifndef _MIN_HEAP_TIMER_H
#define _MIN_HEAP_TIMER_H

#include<netinet/in.h>
#include<time.h>


//定时器类
class heap_timer
{
public:
    heap_timer(int delay)
    {
        expire = time(NULL) + delay;
    }
public:
    time_t expire;  //定时器生效的绝对时间
    void (*cb_func)(int *sockfd);   //定时器回调函数
    int *sockfd;  //连接socket
};


//时间堆类
class time_heap
{
public:
    //构造函数之一，初始化为一个大小为cap的空堆
    time_heap(int cap);

    //构造函数之二，用已有数组初始化堆
    time_heap(heap_timer **init_array, int size, int capacity);

    //销毁时间堆
    ~time_heap();

public:
    //添加目标定时器timer
    void add_timer(heap_timer *timer);

    //删除目标定时器timer
    void del_timer(heap_timer *timer);

    //获得堆顶部的定时器
    heap_timer *top() const;

    //删除堆顶部的定时器
    void pop_timer();
    
    //调整定时器的值
    void adjust_timer(heap_timer *timer, time_t value);

    //心搏函数
    void tick();

    bool empty() const { return cur_size == 0; }

private:
    //最小堆的下虑操作，它确保堆数组中以第hole个节点作为根的子树拥有最小堆性质
    void percolate_down(int hole);

    //将堆数组容量扩大一倍
    void resize();

private:
    heap_timer **array;  //堆数组
    int capacity;   //堆数组的容量
    int cur_size;   //堆数组当前包含的元素的个数
};
#endif
