#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/sendfile.h>

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;

#define SIZE (1024 * 4)

typedef struct FirstLine {
    char* method;
    char* url_path;
    char* query_string;
    char* version;
}FirstLine;

typedef struct Header {
    char header[SIZE];
    long long content_length;
    char* content_type;
    char* host;
    char* user_agent;
    char* cookie;
}Header;

// 初始化首行
void InitFirstLine(FirstLine* first);

// 初始化header
void InitHeader(Header* header);

// 从 socket 中读取一行指令
ssize_t ReadLine(int sock, char output[] , ssize_t max_size);

// 字符串切分
int Split(char input[] , const char* split_char , char* output[]);

// 解析首行，得到 方法 + url + version
int ParseFirstLine(FirstLine* first , char first_line[]);

// 解析 url
int ParseUrl(char url[] , char** p_url_path , char** p_query_string);

// 解析 header
int ParseHeader(FirstLine* first , Header* header);

// 错误处理
void Handler404(int sock);

// 判断是否为目录
int IsDir(const char* file_path);

// 处理静态文件路径  url_path
void HandlerFilePath(const char* url_path , char file_path[]);

// 得到文件大小
ssize_t GetFileSize(const char* file_path);

// 写入静态文件
int WriteStaticFile(int sock , const char* file_path);

// 处理静态文件
int HandlerStaticFile(int sock , const FirstLine* first);

// 处理父进程
int HandlerCGIFather(int sock , int father_read , int father_write , 
                     FirstLine* first , Header* header);

// 处理子进程
int HandlerCGIChild(int child_read , int child_write , 
                    FirstLine* first , Header* header);

// 处理动态页面
int HandlerCGI(int  sock , FirstLine* first , Header* header);

// 构造响应
void HandlerResponse(int sock , FirstLine* first , Header* heaer);

// 处理请求
void HandlerRequest(int64_t sock);

// 封装请求处理函数
void* ThreadEntry(void* arg);

// 初始化服务器  进入事件循环
void HttpServerInit(const char* ip , short port);


