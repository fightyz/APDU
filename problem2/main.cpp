#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

using namespace std;

DIR * rootdir;	// 需要扫描的目录
#define SIZE 50

void readDirProcess(const char *start_dir);	//读进程执行的函数
void statisticProcess();	//单词统计进程执行的函数
pid_t wait_all_subProcess(int *status);	//回收所有的子进程
void readFile(const char *filename);	//读进程中真正执行杜稳健的函数
void recordResult();	//记录统计结果到文件中

ofstream ofile;	//记录统计结构的文件

map<string, int> statisticMap;	//用于保存单词和单词数的Map

int filedes[2];	//匿名管道

int main(int argc, char **argv) {
	struct dirent *dp;	//用于遍历目录
	vector<pid_t> readDirProcessIDs;	//保存读进程的pid

	pid_t pid;
	if(argc != 3) {	//检查命令行参数
		cerr << "Usage wordscan <scan_dir> <result_filename>" << endl;
		_exit(EXIT_FAILURE);
	}
	ofile.open(argv[2]);	//打开记录统计结果的文件
	if(!ofile) {
		cerr << "open result file error!" << endl;
		_exit(EXIT_FAILURE);
	}
	if((rootdir = opendir(argv[1])) == NULL) {	//打开所要扫描的目录
		cerr << "open dir " << argv[1] << " failed!" << endl;
		_exit(EXIT_FAILURE);
	}

	if(pipe(filedes) == -1) {	//创建匿名管道
		cerr << "create pipi error!" << endl;
		_exit(EXIT_FAILURE);
	}

	if((pid = fork()) == -1) {	//创建单词统计进程
		cerr << "fork error!" << endl;
		_exit(EXIT_FAILURE);
	} else if(pid == 0) {
		statisticProcess();	//子进程：单词统计进程执行的函数
	} else {
		chdir(argv[1]);	//将当前进程的工作目录改变到所要扫描的根目录下
		while((dp = readdir(rootdir)) != NULL)	//遍历指定的需要扫描的根目录
		{
			if(dp->d_type & DT_DIR) {	//表示读取到的文件类型是目录
				//如果是当前目录和上一级目录则跳过
				if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
					continue;

				if((pid = fork()) == -1) {	//对根目录下每一个子目录都创建一个进程
					cerr << "fork readDirProcess error!" << endl;
					_exit(EXIT_FAILURE);
				} else if(pid == 0) {	//子进程：遍历子目录
					readDirProcess(dp->d_name);
					return 0;
				} else {	//将子进程的pid保存到向量readDirProcessIDs中
					readDirProcessIDs.push_back(pid);
				}
			} else if(dp->d_type & DT_REG) {	//表示读取到的文件类型是普通文件
				readFile(dp->d_name);	// 是普通文件，则直接读取文件扫描单词
			}
		}

		// 等待所有读子目录的子进程完成
		vector<pid_t>::iterator iter;
		for(iter = readDirProcessIDs.begin(); iter != readDirProcessIDs.end(); iter++) {
			waitpid(*iter, NULL, 0);
		}

		//父进程用不到管道，可以关闭
		close(filedes[0]);	//关闭读端
		close(filedes[1]);	//关闭写端

		//等待所有子进程结束，事实上应该只剩下汇总统计单词的进程了
		while(wait_all_subProcess(NULL) > 0);
		closedir(rootdir);
		ofile.close();
		cout << "OK!" << endl;
		return 0;
	}
}

/*
 *	一直等待子进程，直到子进程退出或者没有子进程了
 *	wait返回错误有两种情况，一种是没有子进程了，一种是被信号处理函数中断
 *	如果是被信号处理函数中断 errno == EINTR 则继续执行wait
 *
 */
pid_t wait_all_subProcess(int *status) {
	int child;
	while(((child = wait(status)) == -1) && (errno == EINTR));
	return child;
}

/*
 *	子进程：汇总单词并统计
 *
 */
void statisticProcess() {
	cout << "statisticing ..." << endl;

	char buf[SIZE];	//从管道中读出数据块保存到这个buf中
	char pre_half_word[SIZE];	//上个数据块结尾的单词不完全时暂存在这里
	char cur_word[SIZE];	//保存一个单词

	memset(buf, 0x00, sizeof(buf));
	memset(pre_half_word, 0x00, sizeof(pre_half_word));
	memset(cur_word, 0x00, sizeof(cur_word));

	map<string, int>:: iterator iter;
	int temp_num = 0;
	int rd_byte_num;	//read实际读出的字节数
	int buf_index;	//当前已读buf的游标
	int cur_word_index;	//当前cur_word的游标
	bool is_half_word = false;	//开始读buf时是否有上个块的半个单词

	close(filedes[1]);	//关闭管道写端

	//从管道中一次最大读取 SIZE 字节的数据到 buf 中，并从 buf 中分离出单词
	while((rd_byte_num = read(filedes[0], buf, SIZE)) > 0) {
		buf_index = 0;
		cur_word_index = 0;
		// cout << "read " << rd_byte_num << " bytes word: " << buf << endl;
		while(buf_index < rd_byte_num) {
			if(is_half_word) {	//如果有半个单词没读完，则先复制那半个单词
				strcpy(cur_word, pre_half_word);
				cur_word_index = strlen(cur_word);
				is_half_word = false;
			}
			if(buf[buf_index] != '\0') {	//从buf中分离出单词
				cur_word[cur_word_index] = buf[buf_index];
				cur_word_index ++;
				buf_index ++;
			} else {	//读完一个完整的单词了，统计其词频
				cur_word[cur_word_index] = '\0';
				iter = statisticMap.find(cur_word);
				if(iter != statisticMap.end()) {
					temp_num = (*iter).second;
					temp_num ++;
					statisticMap[cur_word] = temp_num;
				} else {
					statisticMap[cur_word] = 1;
				}
				cur_word_index = 0;
				buf_index ++;
			}
		}
		if(cur_word_index != 0) {	//读完当前块，但有不完整的单词
			cur_word[cur_word_index] = '\0';
			strcpy(pre_half_word, cur_word);
			is_half_word = true;
		}
	}
	recordResult();	//将统计结果写入文件
	close(filedes[0]);
}

/*
 *	将统计结果写入文件
 *
 */
void recordResult() {
	map<string, int>::iterator iter;
	cout << "write result to file ..." << endl;
	for(iter = statisticMap.begin(); iter != statisticMap.end(); iter++)
	{
		ofile << (*iter).first << "	" << (*iter).second << endl;
	}
	cout << "finished writing!" << endl;
}

/*
 *	子进程递归遍历子目录
 *
 */
void readDirProcess(char const *start_dir) {
	DIR * curdir;
	struct dirent *curdp;
	if((curdir = opendir(start_dir)) == NULL) {
		cerr << "open dir " << start_dir << " failed!" << endl;
		_exit(EXIT_FAILURE);
	}
	chdir(start_dir);	//切换当前的工作目录
	char * current_dir = getcwd(NULL, 0);	//获取当前工作目录，并保存，方便最后返回到该目录
	cout << "Dir: " << start_dir << endl;
	//遍历当前目录中的文件
	while((curdp = readdir(curdir)) != NULL) {
		if(curdp->d_type & DT_DIR) {	//如果是目录，递归遍历之
			if(strcmp(curdp->d_name, ".") == 0 || strcmp(curdp->d_name, "..") == 0)
				continue;
			readDirProcess(curdp->d_name);
			chdir(current_dir);
		} else if(curdp->d_type & DT_REG) {	//如果是普通文件
			readFile(curdp->d_name);
		}
	}
	close(filedes[1]);	//关闭写端
	free(current_dir);
}

 /*
  *	读一个文件并统计其中的单词
  *
  */
void readFile(char const *filename) {
	if(filename[0] == '.') {
		return;
	}
	bool is_end_flag = false;
	FILE *fp = fopen(filename, "r");
	char buf[SIZE];
	int wr_byte_num;

	cout << "readFile: " << filename << endl;
	
	if(!fp) {
		cerr << "open file " << filename << " error!" << endl;
		_exit(EXIT_FAILURE);
	}
	
	//依据分隔符分离出单词，每个单词都带有结束符'\0'，然后用write写入到管道中
	while(fscanf(fp, "%[^,.!?:;*'—-/_|()&^\"\n\r ]", buf) != EOF) {
		getc(fp);
		if(buf[0] == '\0')
			continue;
		if(buf[0] >= '0' && buf[0] <= '9')
			continue;
		buf[0] = tolower(buf[0]);
		wr_byte_num = write(filedes[1], buf, strlen(buf) + 1);
		// cout << "write " << wr_byte_num << "bytes word: " << buf << endl;
		buf[0] = '\0';
		
	}
	fclose(fp);
}