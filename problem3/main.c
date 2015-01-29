#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>

//模拟bool变量
#define bool int
#define false 0
#define true 1

//请求线程数
#define REQUEST_NUM 100000

//计算线程数
#define SERVER_NUM 100

//参加计算的两个数的数据结构：A，B
typedef struct cal_nums {
	int num_a;	//参加计算的A
	int num_b;	//参加计算的B
}cal_nums;

//计算线程所需的数据结构
typedef struct cal_info {
	cal_nums* cal_num_p;			//指向参加计算的参数结构的指针
	int request_threads_index;		//请求线程的索引号
}cal_info;

// 单个任务的结构
typedef struct thpool_job_t {
	void* (*function)(void* arg);	//执行任务的回调函数指针
	void* arg;						//回调函数参数的指针
	struct thpool_job_t* next;		//任务队列中下一个任务的指针
	struct thpool_job_t* prev;		//任务队列中上一个任务的指针
	int request_threads_index;		//请求线程的索引号
	int thread_index;				//计算线程的索引号
}thpool_job_t;

//单个线程任务队列的结构
typedef struct thpool_jobqueue {
	thpool_job_t* head;	//指向队列头的指针
	thpool_job_t* tail;	//指向队列尾的指针
	int jobs_num;		//队列中的任务数
	pthread_cond_t* queue_cond;	//队列资源的条件信号量
	pthread_mutex_t* mutex;	//队列的锁
}thpool_jobqueue;

//线程池的结构
typedef struct thpool_t {
	pthread_t* threads;			//保存线程池中所有线程号的指针
	int threads_num;			//线程池中的线程数
	thpool_jobqueue** jobqueue;	//任务队列的数组，数组中每一项都是对应线程所属的任务队列
}thpool_t;

//每一个线程工作函数需要的数据
typedef struct thread_data{
	thpool_t* thread_pool;				//线程池的指针
	int thread_index;					//计算线程的索引号
	int request_threads_index;			//请求线程的索引号
}thread_data;

//存放计算结果的数据结构
typedef struct result_data{
	int result;			//计算结果
	bool have_result;	//判断当前计算结果是否可用
}result_data;

bool thpool_keepalive = true;	//是否销毁线程的开关

int count = 0;	//请求线程所发送的总的线程数

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;	//用于互斥地访问计算线程的各个队列
pthread_mutex_t finished_lock = PTHREAD_MUTEX_INITIALIZER;	//用于互斥访问已完成的请求线程数目finished_num
pthread_cond_t finished_cond = PTHREAD_COND_INITIALIZER;	//用于判断已完成请求线程数目的条件变量

int finished_num = 0;	//已完成的请求线程数目

thpool_t* thread_pool;	//线程池指针
result_data* results;	//存放计算结果的数组的指针

/*
 *	@brief 初始化线程池
 *
 *	@param 线程池的线程数
 */
thpool_t* thpool_init(int threads_num);

/*	@brief 销毁线程池
 *
 *	@param 线程池的指针
 */
void thpool_destroy(thpool_t* thread_pool);

/*	@brief 线程池的计算线程执行函数
 *	
 *	@param 计算线程所需数据结构
 */
void thpool_thread_do(thread_data* th_data);

/*	@brief 请求线程向线程池发送计算请求
 *
 *	@param 线程池的指针
 *	@param 请求计算任务的回调函数指针
 *	@param 回调函数参数的指针
 *
 */
int thpool_add_work(thpool_t* thread_pool, void *(*function_p)(void *), void* arg_p);

/*	@brief 初始化各个计算线程的工作队列
 *
 *	@param 线程池的指针
 */
int thpool_jobqueue_init(thpool_t* thread_pool);

/*	@brief 移除一个计算线程的工作队列中最后一个工作
 *
 *	@param 指向线程池的指针
 *	@param 计算线程的索引号
 */
int thpool_jobqueue_removelast(thpool_t* thread_pool, int thread_index);

/*	@brief 从一个计算线程的工作队列中取出最后一个工作
 *
 *	@param 指向线程池的指针
 * 	@param 计算线程的索引号
 */
thpool_job_t* thpool_jobqueue_peek(thpool_t* thread_pool, int thread_index);

/*	@brief 新增一个工作到工作队列
 *
 *	@param 指向一个工作的数据结构的指针
 */
void thpool_jobqueue_add(thpool_job_t* newjob_p);

/*	@brief 清空线程池中所有计算线程的工作队列
 *
 *	@param 指向线程池的指针
 */
void thpool_jobqueue_empty(thpool_t* thread_pool);

/*	@brief 初始化每一个计算线程的工作队列的条件变量
 *
 *	@param 指向线程池的指针
 */
int thpool_cond_init(thpool_t* thread_pool);

/*	@brief 初始化每一个计算线程的工作队列的互斥量
 *
 *	@param 指向线程池的指针
 */
int thpool_mutex_init(thpool_t* thread_pool);

/*	@brief 计算加法的任务函数
 *
 *	@param 计算所需数据结构的指针
 */
int task_add(cal_nums* args);

/*	@brief 计算减法的任务函数
 *
 *	@param 计算所需数据结构的指针
 */
int task_sub(cal_nums* args);

/*	@brief 计算乘法的任务函数
 *
 *	@param 计算所需的数据结构的指针
 */
int task_mul(cal_nums* args);

/*	@brief 计算除法的任务函数
 *
 *	@param 计算所需的数据结构的指针
 */
int task_div(cal_nums* args);

/*	@brief 请求线程的执行函数
 *
 *	@param 计算所需数据结构的指针
 */
void request_do(cal_info* args);

/*	@brief 请求线程的初始化
 *
 *	@param 请求线程的数量
 * 	@param 指向保存请求线程的线程号的数组的指针
 */
void request_threads_init(int threads_num, pthread_t* request_threads);

int main() {
	srand(time(NULL));

	//构造计算线程池
	thread_pool = thpool_init(SERVER_NUM);

	puts("Thread Pool created.");

	//保存请求线程号
	pthread_t* request_threads;

	//创建请求线程及所需资源
	request_threads_init(REQUEST_NUM, request_threads);

	//使用条件变量阻塞以等待所有请求线程都收到计算结果并返回
	pthread_mutex_lock(&finished_lock);
	while(finished_num != REQUEST_NUM) {
		pthread_cond_wait(&finished_cond, &finished_lock);
	}
	pthread_mutex_unlock(&finished_lock);

	puts("Will kill threadpool");

	//销毁计算线程池
	thpool_destroy(thread_pool);

	//free掉请求线程分配的资源
	free(request_threads);
	free(results);

	return 0;
}

// 建立计算线程池
thpool_t* thpool_init(int threads_num) {
	thpool_t* thread_pool;

	//不足一个线程则默认一个线程
	if(!threads_num || threads_num < 1)
		threads_num = 1;

	//为线程池的数据结构分配空间
	thread_pool = (thpool_t*)malloc(sizeof(thpool_t));
	if(thread_pool == NULL) {
		fprintf(stderr, "thpool_init(): Could not allocate memory for thread pool\n");
		return NULL;
	}

	//为线程池的线程号数组分配空间
	thread_pool->threads = (pthread_t*)malloc(threads_num * sizeof(pthread_t));
	if (thread_pool->threads == NULL){
		fprintf(stderr, "thpool_init(): Could not allocate memory for thread IDs\n");
		return NULL;
	}
	thread_pool->threads_num = threads_num;

	//初始化每个计算线程的工作队列数组
	if(thpool_jobqueue_init(thread_pool) == -1) {
		fprintf(stderr, "thpool_init(): Could not allocate memory for job queue\n");
		return NULL;
	}

	//初始化每一个计算线程的条件变量
	if(thpool_cond_init(thread_pool) == -1) {
		fprintf(stderr, "thpool_initi(): Could not create condition variable\n");
		return NULL;
	}

	//为线程池中各个线程初始化锁
	if(thpool_mutex_init(thread_pool) == -1) {
		fprintf(stderr, "thpool_init(): Could not create mutexs\n");
		return NULL;
	}

	//计算线程所需数据结构的指针数组
	thread_data* th_data[threads_num];

	//构造线程池
	for(int i = 0; i < threads_num; i++) {
		th_data[i] = (thread_data*)malloc(sizeof(thread_data));
		th_data[i]->thread_pool = thread_pool;
		th_data[i]->thread_index = i;
		pthread_create(&(thread_pool->threads[i]), NULL, (void *)thpool_thread_do, (void *)th_data[i]);
	}

	return thread_pool;
}

void thpool_destroy(thpool_t* thread_pool) {

	thpool_keepalive = false;

	for(int i = 0; i < (thread_pool->threads_num); i++) {
		if(pthread_cond_signal(thread_pool->jobqueue[i]->queue_cond)) {
			fprintf(stderr, "thpool_destroy(): Could not bypass pthread_cond_wait()\n");
		}
	}

	for(int i = 0; i < (thread_pool->threads_num); i++) {
		if(pthread_cond_destroy(thread_pool->jobqueue[i]->queue_cond)) {
			fprintf(stderr, "thpool_destroy(): Could not destroy cond sem\n");
		}
	}

	for(int i = 0; i < (thread_pool->threads_num); i++) {
		if(pthread_mutex_unlock(thread_pool->jobqueue[i]->mutex)) {
			fprintf(stderr, "thpool_destroy(): Could not bypass pthread_mutex_lock()\n");
		}
	}

	for(int i = 0; i < (thread_pool->threads_num); i++) {
		if((errno = pthread_mutex_destroy(thread_pool->jobqueue[i]->mutex)) != 0) {
			fprintf(stderr, "thpool_destroy(): Could not destroy mutex %d: %s\n", i, strerror(errno));
		}
	}

	for(int i = 0; i < (thread_pool->threads_num); i++) {
		pthread_join(thread_pool->threads[i], NULL);
	}

	thpool_jobqueue_empty(thread_pool);

	free(thread_pool->threads);
	for(int i = 0; i < thread_pool->threads_num; i++) {
		free(thread_pool->jobqueue[i]->mutex);
		free(thread_pool->jobqueue[i]->queue_cond);
		free(thread_pool->jobqueue[i]);
	}
	free(thread_pool->jobqueue);
	free(thread_pool);
}

//计算线程工作的函数
void thpool_thread_do(thread_data* th_data) {
	thpool_t* tp_p = th_data->thread_pool;
	int thread_index = th_data->thread_index;
	int request_threads_index;

	free(th_data);

	//只要不销毁线程就会一直循环，销毁线程时thpool_keepalive = false
	while(thpool_keepalive) {

		if(thpool_keepalive) {
			void*(*func_buff)(void* arg);	//计算线程的计算函数指针
			void* arg_buff;					//计算函数的参数指针
			thpool_job_t* job_p;	//取出工作的指针

			//对工作队列上锁
			pthread_mutex_lock(tp_p->jobqueue[thread_index]->mutex);

			//阻塞以等待工作队列上有工作
			while((job_p = thpool_jobqueue_peek(tp_p, thread_index)) == NULL && thpool_keepalive) {
				pthread_cond_wait(tp_p->jobqueue[thread_index]->queue_cond,
					tp_p->jobqueue[thread_index]->mutex);
			}

			//销毁线程时job_p == NULL, 且thpool_keepalive == false
			if(job_p == NULL) {
                pthread_mutex_unlock(tp_p->jobqueue[thread_index]->mutex);
                // printf("thpool_thread_do: #%d job_p is null. keepalive = %d\n", thread_index, thpool_keepalive);
                continue;
            }

			func_buff = job_p->function;
			arg_buff = job_p->arg;
			request_threads_index = job_p->request_threads_index;

			//从队列中删除已取出的工作
			thpool_jobqueue_removelast(tp_p, thread_index);
			free(job_p);

			//给工作队列解锁
			pthread_mutex_unlock(tp_p->jobqueue[thread_index]->mutex);

			//调用具体计算的回调函数
			int result = func_buff(arg_buff);

			//将计算结果存入结果数组中
			results[request_threads_index].result = result;
			results[request_threads_index].have_result = true;

			// printf("thpool_thread_do: Thread #%d finished request #%d\n", thread_index, request_threads_index);
		} else {
			// free(th_data); 
			return;
		}
	}
	// printf("thpool_thread_do: Thread #%d is finished\n", thread_index);
	return;
}

//将计算任务负载均衡地加入到计算线程的工作队列中
int thpool_add_work(thpool_t* thread_pool, void *(*function_p)(void *), void* arg_p) {
	thpool_job_t* newJob;
	int threads_num = thread_pool->threads_num;

	//为新任务分配空间
	newJob = (thpool_job_t*)malloc(sizeof(thpool_job_t));
	if (newJob == NULL){
		fprintf(stderr, "thpool_add_work(): Could not allocate memory for new job\n");
		exit(1);
	}

	//初始化新任务的各项信息，包括回调函数及其参数，请求线程的索引号
	newJob->function = function_p;
	newJob->arg = (void*)((cal_info*)arg_p)->cal_num_p;
	newJob->prev = NULL;
	newJob->next = NULL;
	newJob->request_threads_index = ((cal_info*)arg_p)->request_threads_index;;

	//根据总的请求计算的任务数量，负载均衡地分配给线程池中各个计算线程
	pthread_mutex_lock(&mutex);
	printf("count = %d =================================\n", count);
	count++;
	newJob->thread_index = count % threads_num;
	pthread_mutex_unlock(&mutex);

	// newJob->thread_index = rand() % threads_num;

	//将新工作加入到工作队列中
	thpool_jobqueue_add(newJob);

	return 0;
}


//初始化任务队列数组
int thpool_jobqueue_init(thpool_t* thread_pool) {
	printf("thpool_jobqueue_init\n");
	int threads_num = thread_pool->threads_num;
	thread_pool->jobqueue = (thpool_jobqueue**)malloc(threads_num * sizeof(thpool_jobqueue*));
	if(thread_pool->jobqueue == NULL)
		return -1;

	for(int i = 0; i < threads_num; i++) {
		thread_pool->jobqueue[i] = (thpool_jobqueue*)malloc(sizeof(thpool_jobqueue));
		thread_pool->jobqueue[i]->head = NULL;
		thread_pool->jobqueue[i]->tail = NULL;
		thread_pool->jobqueue[i]->jobs_num = 0;
		thread_pool->jobqueue[i]->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		thread_pool->jobqueue[i]->queue_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	}

	return 0;
}

//将任务从队列中移除
int thpool_jobqueue_removelast(thpool_t* thread_pool, int thread_index) {

	thpool_job_t* oldLastJob;
	oldLastJob = thread_pool->jobqueue[thread_index]->tail;

	switch(thread_pool->jobqueue[thread_index]->jobs_num) {

		case 0:
			return -1;
			break;

		case 1:
			thread_pool->jobqueue[thread_index]->tail = NULL;
			thread_pool->jobqueue[thread_index]->head = NULL;
			break;

		default:
			oldLastJob->prev->next = NULL;
			thread_pool->jobqueue[thread_index]->tail = oldLastJob->prev;
	}

	(thread_pool->jobqueue[thread_index]->jobs_num)--;

	return 0;
}

//获得任务队列中排最后的任务
thpool_job_t* thpool_jobqueue_peek(thpool_t* thread_pool, int thread_index) {

	return thread_pool->jobqueue[thread_index]->tail;
}

//将任务加入到一个计算线程的队列中
void thpool_jobqueue_add(thpool_job_t* newjob_p) {
	int thread_index = newjob_p->thread_index;

	//锁住工作队列
	if((errno = pthread_mutex_lock(thread_pool->jobqueue[thread_index]->mutex)) != 0) {
		fprintf(stderr, "thpool_jobqueue_add: Could not lock: %s\n", strerror(errno));
		return;
	}
	thpool_job_t* oldFirstJob;
	oldFirstJob = thread_pool->jobqueue[thread_index]->head;

	//将新的工作加入到工作队列中
	switch(thread_pool->jobqueue[thread_index]->jobs_num) {
		case 0:
			thread_pool->jobqueue[thread_index]->tail = newjob_p;
			thread_pool->jobqueue[thread_index]->head = newjob_p;
			break;

		default:
			oldFirstJob->prev = newjob_p;
			newjob_p->next = oldFirstJob;
			thread_pool->jobqueue[thread_index]->head = newjob_p;
	}

	(thread_pool->jobqueue[thread_index]->jobs_num)++;
	printf("thpool_jobqueue_add(): Thread %d jobs_num = %d\n", thread_index, thread_pool->jobqueue[thread_index]->jobs_num);
	pthread_mutex_unlock(thread_pool->jobqueue[thread_index]->mutex);

	//给计算线程发送信号改变条件变量状态
	pthread_cond_signal(thread_pool->jobqueue[thread_index]->queue_cond);
}

//移除所有计算线程的任务队列
void thpool_jobqueue_empty(thpool_t* thread_pool) {
	thpool_job_t* curjob;

	for(int i = 0; i < thread_pool->threads_num; i++) {
		curjob = thread_pool->jobqueue[i]->tail;

		while(thread_pool->jobqueue[i]->jobs_num) {
			thread_pool->jobqueue[i]->tail = curjob->prev;
			free(curjob);
			curjob = thread_pool->jobqueue[i]->tail;
			thread_pool->jobqueue[i]->jobs_num--;
		}

		thread_pool->jobqueue[i]->head = NULL;
		thread_pool->jobqueue[i]->tail = NULL;
	}
}

//初始化每个线程的条件信号量
int thpool_cond_init(thpool_t* thread_pool) {
	printf("thpool_cond_init\n");
	int threads_num = thread_pool->threads_num;

	for(int i = 0; i < threads_num; i++) {
		if((errno = pthread_cond_init(thread_pool->jobqueue[i]->queue_cond, NULL)) != 0) {
			fprintf(stderr, "thpool_cond_init: Could not init condition variable: %s\n", strerror(errno));
			return -1;
		}
	}
	return 0;
}

//初始化每个线程队列的锁
int thpool_mutex_init(thpool_t* thread_pool) {
	printf("thpool_mutex_init()\n");
	int threads_num = thread_pool->threads_num;
	for(int i = 0; i < threads_num; i++) {
		if(pthread_mutex_init(thread_pool->jobqueue[i]->mutex, NULL))
			return -1;
	}
	return 0;
}

//加法回调函数
int task_add(cal_nums *args) {
	int result = args->num_a + args->num_b;
	// printf("# Thread working: %u at %d ADD %d = %d\n",
	// 	(unsigned int)pthread_self(), args->num_a, args->num_b, result);
	return result;
}

//减法回调函数
int task_sub(cal_nums *args) {
	int result = args->num_a - args->num_b;
	// printf("# Thread working: %u at %d SUB %d = %d\n",
	// 	(unsigned int)pthread_self(), args->num_a, args->num_b, result);
	return result;
}

//乘法回调函数
int task_mul(cal_nums *args) {
	int result = args->num_a * args->num_b;
	// printf("# Thread working: %u at %d MUL %d = %d\n",
	// 	(unsigned int)pthread_self(), args->num_a, args->num_b, result);
	return result;
}

//除法回调函数
int task_div(cal_nums *args) {
	int result = args->num_a / args->num_b;
	// printf("# Thread working: %u at %d DIV %d = %d\n",
	// 	(unsigned int)pthread_self(), args->num_a, args->num_b, result);
	return result;
}

//请求线程的执行函数
void request_do(cal_info* args) {
	//随机生成两个100以内的操作数
	args->cal_num_p->num_a = rand() % 100 + 1;
	args->cal_num_p->num_b = rand() % 100 + 1;

	int request_threads_index = args->request_threads_index;

	void*(*func_buff)(void* arg);
	void* arg_buff;

	arg_buff = args;

	//随机选择加减乘除
	switch(rand() % 4) {
		case 0:
			func_buff = (void*)task_add;
			// printf("Request Thread #%u start at index: %d do: %d ADD %d\n", 
			// 	(unsigned)pthread_self(), args->request_threads_index, args->cal_num_p->num_a, args->cal_num_p->num_b);
			break;

		case 1:
			func_buff = (void*)task_sub;
			// printf("Request Thread #%u start at index: %d do: %d SUB %d\n", 
			// 	(unsigned)pthread_self(), args->request_threads_index, args->cal_num_p->num_a, args->cal_num_p->num_b);
			break;

		case 2:
			func_buff = (void*)task_mul;
			// printf("Request Thread #%u start at index: %d do: %d MUL %d\n", 
			// 	(unsigned)pthread_self(), args->request_threads_index, args->cal_num_p->num_a, args->cal_num_p->num_b);
			break;

		case 3:
			func_buff = (void*)task_div;
			// printf("Request Thread #%u start at index: %d do: %d DIV %d\n", 
			// 	(unsigned)pthread_self(), args->request_threads_index, args->cal_num_p->num_a, args->cal_num_p->num_b);
			break;

		default:
			;
	}

	//向计算线程的工作队列添加任务
	thpool_add_work(thread_pool, (void*)func_buff, (void*)arg_buff);

	//等待计算线程将计算结果放入results数组
	while(!results[request_threads_index].have_result) {
		usleep(10);
	}
	printf("Request Thread %d get result : %d\n", args->request_threads_index, results[request_threads_index].result);

	//完成的请求线程数+1
	pthread_mutex_lock(&finished_lock);
	finished_num++;
	pthread_mutex_unlock(&finished_lock);

	//通知主线程，以便主线程检查是否所有的请求线程都已完成
	pthread_cond_signal(&finished_cond);

	printf("request_do(): Request Thread %d exit.\n", args->request_threads_index);

	free(args->cal_num_p);
	free(args);

	// pthread_exit(NULL);
	return;
}

//创建请求线程
void request_threads_init(int threads_num, pthread_t* request_threads) {
	request_threads = (pthread_t*)malloc(threads_num * sizeof(pthread_t));

	//创建结果存储区，以便请求线程取得结果
	results = (result_data*)malloc(threads_num * sizeof(result_data));
	memset(results, 0x00, threads_num * sizeof(result_data));

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	//为每个请求线程的计算信息分配空间
	for(int i = 0; i < threads_num; i++) {
		cal_info* args = (cal_info*)malloc(sizeof(cal_info));
		args->cal_num_p = (cal_nums*)malloc(sizeof(cal_nums));
		args->request_threads_index = i;
		printf("request_threads_init(): Request Thread %d create.\n", i);
		pthread_create(&(request_threads[i]), &attr, (void*)request_do, (void*)args);
	}
	pthread_attr_destroy(&attr);
}








