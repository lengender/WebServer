/*************************************************************************
	> File Name: min_heap_timer.cpp
	> Author: 
	> Mail: 
	> Created Time: 2017年02月22日 星期三 15时07分26秒
 ************************************************************************/

#include<iostream>
#include<netinet/in.h>
#include<time.h>
#include<exception>
#include"min_heap_timer.h"

time_heap::time_heap(int cap) 
    :capacity(cap), cur_size(0)
{
    array = new heap_timer*[capacity];  //创建堆数组
    if(!array){
        throw std::exception();
    }

    for(int i = 0; i < capacity; ++i){
        array[i] = NULL;
    }
}


time_heap::time_heap(heap_timer **init_array, int size, int capacity)
        :cur_size(size), capacity(capacity)
{
    if(capacity < size){
        throw std::exception();
    }

    array = new heap_timer*[capacity];  //创建堆数组
    if(!array){
        throw std::exception();
    }

    for(int i = 0; i < capacity; ++i){
        array[i] = NULL;
    }

    if(size != 0){
        //初始化堆数组
        for(int i = 0; i < size; ++i)
        {
            array[i] = init_array[i];
        }

        for(int i = (cur_size - 1) / 2; i >= 0; --i)
        {
            //对数组中的第[(cur_size - 1)/ 2 ~ 0]个元素执行下虑操作
            percolate_down(i);
        }
    }
}

time_heap::~time_heap()
{
    for(int i = 0; i < cur_size; ++i){
        if(array[i])
            delete array[i];
    }

    delete[] array;
}

void time_heap::add_timer(heap_timer* timer)
{
    if(!timer)
    {
        return ;
    }

    if(cur_size >= capacity)  //如果当前堆数组的容量不够，则将其扩大一倍
    {
        resize();
    }

    //新插入了一个元素，当前堆大小加1，hole是新建空穴的位置
    int hole = cur_size++;
    int parent = 0;

    //从空穴到根节点的路径上的所有节点指向上虑操作
    for(; hole > 0; hole = parent)
    {
        parent = (hole - 1) / 2;

        if(array[parent]->expire <= timer->expire)
        {
            break;
        }

        array[hole] = array[parent];
    }
    array[hole] = timer;
}


void time_heap::del_timer(heap_timer* timer)
{
    if(!timer)
    {
        return;
    }

    //仅仅将目标定时器的回调函数设置为空，即所谓的延迟销毁，这将节省真正
    //删除该定时器造成的开销，但这样做容易是堆数组膨胀
    timer->cb_func = NULL;
}

heap_timer* time_heap::top() const
{
    if(empty())
    {
        return NULL;
    }

    return array[0];
}

void time_heap::pop_timer()
{
    if(empty())
    {
        return;
    }

    if(array[0])
    {
        delete array[0];
        
        //将原来的堆顶元素替换为堆数组中最后一个元素
        array[0] = array[--cur_size];
        percolate_down(0);   //对新的堆顶元素执行下虑操作

        if(array[0] && !array[0]->cb_func)
        {
            pop_timer();
        }
    }
}

void time_heap::adjust_timer(heap_timer *timer, time_t value)
{
    heap_timer *tmp = new heap_timer(value);
    tmp->sockfd = timer->sockfd;
    tmp->cb_func = timer->cb_func;
    timer->sockfd = NULL;
    timer->cb_func = NULL;
    add_timer(tmp);
}

void time_heap::tick()
{
    heap_timer *tmp = array[0];
    time_t cur = time(NULL);  //循环预处理堆中的到期的定时器

    while(!empty())
    {
        if(!tmp){
            break;
        }

        //如果堆顶定时器没有到期，则退出循环
        if(tmp->expire > cur)
        {
            break;
        }

        //否则执行堆顶定时器中的任务
        if(array[0]->cb_func)
        {
            array[0]->cb_func(array[0]->sockfd);
        }

        //将堆顶元素删除，同时生成新的堆顶定时器
        pop_timer();
        tmp = array[0];
    }
}

void time_heap::percolate_down(int hole)
{
    heap_timer *temp = array[hole];
    int child = 0;

    for(; (hole * 2 + 1) <= (cur_size - 1); hole = child)
    {
        child = hole * 2 + 1;

        if((child < (cur_size - 1)) && (array[child + 1]->expire < array[child]->expire))
        {
            ++child;
        }

        if(array[child]->expire < temp->expire)
        {
            array[hole] = array[child];
        }
        else{
            break;
        }
    }

    array[hole] = temp;
}


//将数组容量扩大一倍
void time_heap::resize() 
{
    heap_timer **temp = new heap_timer*[2 * capacity];

    if(!temp)
    {
        throw std::exception();
    }

    for(int i = 0; i < 2 * capacity; ++i)
    {
        temp[i] = NULL;
    }

    capacity = 2 * capacity;
    for(int i = 0; i < cur_size; ++i)
    {
        temp[i] = array[i];
    }

    delete[] array;
    array = temp;
}

