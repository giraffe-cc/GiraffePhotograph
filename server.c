#include "server.h"

void InitFirstLine(FirstLine* first) {
    first->method = NULL;
    first->url_path = NULL;
    first->query_string = NULL;
    first->version = NULL;
}

void InitHeader(Header* header) {
    header->content_length = 0;
    header->content_type = NULL;
    header->host = NULL;
    header->user_agent = NULL;
}

ssize_t ReadLine(int sock, char output[] , ssize_t max_size) {
    char c = '\0';
    ssize_t i = 0;
    while (i < max_size) {
        ssize_t read_size = recv(sock , &c , 1 , 0);
        if (read_size <= 0) {
            perror("recv\n");
            return -1;
        }
        if (c == '\r') {
            recv(sock , &c , 1 , MSG_PEEK);
            if (c == '\n') {
                recv(sock , &c , 1 , 0);
            }
            else {
                c = '\n';
            }
        }
        output[i++] = c;
        if (c == '\n') {
            break;
        }
    }
    output[i] = '\0';
    return i;
}

//进行字符串切分
int Split(char input[] , const char* split_char , char* output[]) {
    char* tmp = NULL;
    int output_index = 0;
    char* ptr = strtok_r(input , split_char , &tmp);
    while(ptr != NULL) {
        output[output_index++] = ptr;
        ptr = strtok_r(NULL , split_char , &tmp);
    }
    return output_index;
}

/*
int ParseFirstLine(char first_line[] , char** p_method , char** p_url) {
    char* tok[10] = {0};
    int n = Split(first_line , " " , tok);
    if (n != 3) {
        printf ("Split failed! n = %d\n" , n);
        return -1;
    }
    *p_method = tok[0];
    *p_url = tok[1];
    return 0;
}
*/

int ParseFirstLine(FirstLine* first , char first_line[]) {
    char* tok[10] = {0};
    int n = Split(first_line , " " , tok);
    if (n != 3) {
        printf("Split failed! n = %d\n" , n);
        return -1;
    }
    first->method = tok[0];
    first->version = tok[2];
    if (tok[1] == NULL) {
        return -1;
    }
    int i = 0;
    for(; i < (int)strlen(tok[1]) ; ++i) {
        if(*(tok[1]+i) == '?') {
            break;
        }
    }
    if (i == (int)strlen(tok[1])) {
        first->url_path = tok[1];
        first->query_string = NULL;
        }
    else {
        first->url_path = tok[1];
        *((tok[1]) + i) = '\0';
        first->query_string = tok[1] + i + 1;
    }
    return 0;
}

int ParseUrl(char url[] , char** p_url_path , char** p_query_string) {
    *p_url_path = url;
    char* p = url;
    for (; *p != '\0' ; ++p) {
        if (*p == '?') {
            *p = '\0';
            *p_query_string = p + 1;
            return 0;
        }
    }
    *p_query_string =NULL;
    return 0;
}

/*
int ParseHeader(int sock , int* content_lengh) {
    char buf[SIZE] = {0};
    while(1) {
        ssize_t read_size = ReadLine(sock , buf , sizeof(buf) - 1);
        if (read_size <= 0) {
            return -1;
        }
        if (strcmp(buf , "\n") == 0) {
            return 0;
        }
        const char* key = "Content-Length: ";
        if (strncmp(buf , key , strlen("key")) == 0) {
            *content_lengh = atoi(buf + strlen(key));
        }
    }
    return 0;
}
*/

int ParseHeader(FirstLine* first , Header* header) {
    if (strcasecmp(first->method , "POST") == 0) {
        char* content_length = strstr(header->header , "Content-Length: ");
        if (content_length != NULL) {
            content_length += strlen("Content-Length: ");
            char* cur = content_length;
            while(*cur != '\n') {
                ++cur;
            }
            *cur = '\0';
            header->content_length = atoi(content_length);
            *cur = '\n';
        }
        // **************************************************************
        char* content_type = strstr(header->header , "Content-Type: ");
        if (content_type != NULL) {
            content_type += strlen("Content-Type: ");
            char* cur = content_type;
            while(*cur != '\n') {
                ++cur;
            }
            *cur = '\0';
            header->content_type = content_type;
            *cur = '\n';
        }
        // **************************************************************
    }
    char* cookie = strstr(header->header , "Cookie: ");
    if (cookie != NULL) {
        cookie += strlen("Cookie: ");
        char* cur = cookie;
        while(*cur != '\n') {
            ++cur;
        }
        *cur = '\0';
        header->cookie = cookie;
    }
    return 0;
}


void Handler404(int sock) {
    const char* first_line = "Http/1.1 404 Not Found\n";
    const char* blank_line = "\n";
    const char* body = "<head><meta http-equiv=\"Content-Type\""
        " content=\"text/html;charset=utf-8\">"
        "</head><h1> 404!你的页面出错了！ </h1>";
    send(sock , first_line , strlen(first_line) , 0);
    send(sock , blank_line , strlen(blank_line) , 0);
    send(sock , body , strlen(body) , 0);
    return;
}

int IsDir(const char* file_path) {
    struct stat st;
    int ret = stat(file_path , &st);
    if (ret < 0) {
        return 0;
    }
    if (S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

void HandlerFilePath(const char* url_path , char file_path[]) {
    sprintf(file_path , "./wwwroot%s", url_path);
    if (file_path[strlen(file_path) - 1] == '/') {
        strcat(file_path , "index.html");
    }
    else {
        if (IsDir(file_path)) {
            strcat(file_path , "/index.html");
        }
    }
}

ssize_t GetFileSize(const char* file_path) {
    struct stat st;
    int ret = stat(file_path , &st);
    if (ret < 0) {
        return -1;
    }
    return st.st_size;
}

int WriteStaticFile(int sock , const char* file_path) {
    int fd = open(file_path , O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 404;
    }
    /*
    const char* first_line = "HTTP/1.1 200 OK\n";
    send(sock , first_line , strlen(first_line) , 0);
    const char* blank_line = "\n";
    send(sock , blank_line , strlen(blank_line) , 0);
    ssize_t file_size = GetFileSize(file_path);
    sendfile(sock , fd , NULL , file_size);
    */
    ssize_t file_size = GetFileSize(file_path);
    if (file_size == -1) {
        Handler404(sock);
        close(sock);
        return -1;
    }
    char response[SIZE] = {0};
    int len = strlen(file_path) - 1;
    int flag = 0;
    while(len) {
        if (file_path[len] == '.') {
            flag = 1;
            break;
        }
        --len;
    }
    if (flag == 1) {
        if (strcasecmp(file_path+len , ".html") == 0) {
            sprintf(response , "HTTP/1.1 200 OK\nContent-Lenght: %lu\nContent-Type：charset=utf-8 \n\n" , file_size);
        }
        else {
            sprintf(response , "HTTP/1,1 200 OK\nContent-Length: %lu\n\n" , file_size);
        }
    }
    else {
        sprintf(response , "HTTP/1.1 200 OK\nContent-Length: %lu\n\n" , file_size);
    }
    send(sock , response , strlen(response) , 0);
    sendfile(sock , fd , NULL , file_size);
    close(fd);
    return 0;
}

int HandlerStaticFile(int sock , const FirstLine* first) {
    char file_path[SIZE] = {0};
    HandlerFilePath(first->url_path , file_path);
    int err_code = WriteStaticFile(sock , file_path);
    close(sock);
    return err_code;
}

int HandlerCGIFather(int sock , int father_read , int father_write , FirstLine* first , Header* header) {
    if (strcasecmp(first->method , "POST") == 0) {
        char c = '\0';
        int i = 0;
        for (; i < header->content_length ; ++i) {
            read(sock , &c , 1);
            write(father_write , &c , 1);
        }
    }
    /*
    const char* first_line = "HTTP/1.1 200 OK\n";
    send(sock , first_line , strlen(first_line) , 0);
    const  char* blank_line = "\n";
    send(sock , blank_line , strlen(blank_line) , 0);
    char c = '\0';
    while(read(father_read , &c , 1) > 0) {
        write(sock , &c ,1);
    }
    */
    wait(NULL);
    char* buf = (char*)malloc(sizeof(char) * SIZE * 10);
    size_t num = 0;
    char c = '\0';
    while(read(father_read , &c , 1) > 0) {
        buf[num] = c;
        ++num;
    }
    char response[SIZE] = {0};
    if (strcmp(first->url_path , "/wwwroot") != 0) {
        sprintf(response , "HTTP/1.1 200 OK\nContent-Lengh: %lu\n\n" , num);
        send(sock , response , strlen(response) , 0);
    }
    send(sock , buf , num , 0);
    free(buf);
    return 200;
}

int HandlerCGIChild(int child_read , int child_write , FirstLine* first , Header* header) {
    char method_env[SIZE] = {0};
    sprintf(method_env , "REQUEST_METHOD=%s", first->method);
    putenv(method_env);
    if (strcasecmp(first->method , "GET") == 0) {
        char query_string_env[SIZE] = {0};
        sprintf(query_string_env , "QUERY_STRING=%s", first->query_string);
        putenv(query_string_env);
    }
    else {
        char content_length_env[SIZE] = {0};
        sprintf(content_length_env , "CONTENT_LENGTH=%llu" , header->content_length);
        putenv(content_length_env);
    }
    char cookie[SIZE] = {0};
    sprintf(cookie , "HTTP_COOKIE=%s" , header->cookie);
    putenv(cookie);
    dup2(child_read , 0);
    dup2(child_write , 1);
    char file_path[SIZE] = {0};
    HandlerFilePath(first->url_path , file_path);
    execl(file_path , file_path , NULL);
    exit(0);
}

int HandlerCGI(int  sock , FirstLine* first , Header* header) {
    int fd1[2] , fd2[2];
    int cur = pipe(fd1);
    if (cur < 0) {
        perror("pipe1 error\n");
        return -1;
    }
    cur = pipe(fd2);
    if (cur < 0) {
        perror("pipe error\n");
        return -1;
    }
    int father_read = fd1[0];
    int child_write = fd1[1];
    int father_write = fd2[1];
    int  child_read = fd2[0];
    
    pid_t ret = fork();
    if (ret > 0) {
        close(child_read);
        close(child_write);
        HandlerCGIFather(sock , father_read , father_write , first , header);
        close(father_read);
        close(father_write);
    }
    else if (ret == 0) {
        close(father_read);
        close(father_write);
        HandlerCGIChild(child_read , child_write , first , header);
        close(child_read);
        close(child_write);
    }
    else {
        perror("fork");
    }
    return 200;
}

void HandlerResponse(int sock , FirstLine* first , Header* header) {
    if (strcasecmp(first->method , "GET") == 0 && first->query_string == NULL) {
        int ret = HandlerStaticFile(sock , first);
        if (ret < 0) {
            Handler404(sock);
        }
    }
    else if (strcasecmp(first->method , "POST") == 0 && header->content_length == 0) {
        Handler404(sock);
    }
    else if (strcasecmp(first->method , "GET") == 0 && first->query_string != NULL) {
        int ret = HandlerCGI(sock , first , header);
        if (ret < 0) {
            Handler404(sock);
        }
    }
    else if (strcasecmp(first->method , "POST") == 0) {
        int ret = HandlerCGI(sock , first , header);
        if (ret < 0) {
            Handler404(sock);
        }
    }
    else {
        Handler404(sock);
    }
}

// 完成具体的请求处理过程
void HandlerRequest(int64_t sock) {
    /*
    int err_code = 200;
    Request* req;
    memset(&req , 0 , sizeof(req));
    if (ReadLine(sock , req.first_line , sizeof(req.first_line) - 1) < 0) {
        err_code = 404;
        goto END;
    }
    printf("first_line: %s\n" , req.first_line);
    if (ParseFirstLine(req.first_line , &req.method , &req.url) < 0) {
        err_code = 404;
        goto END;
    }
    if (ParseUrl(req.url , &req.url_path , &req.query_string) < 0) {
        err_code = 404;
        goto END;
    }
    if (ParseHeader(sock , &req.content_length)) {
        err_code = 404;
        goto END;
    }
    if (strcasecmp(req.method , "GET") == 0 && req.query_string == NULL) {
        err_code = HandlerStaticFile(sock , &req);
    }
    else if (strcasecmp(req.method , "GET") == 0 && req.query_string != NULL) {
        err_code = HandlerCGI(sock , &req);
    }
    else if (strcasecmp(req.method , "POST") == 0) {
        err_code = HandlerCGI(sock , &req);
    }
    else {
        err_code = 404;
        goto END;
    }
END:
    if (err_code != 200) {
        Handler404(sock);
    }
    */
    printf("=====================\n");
    FirstLine first;
    InitFirstLine(&first);
    char first_line[SIZE] = {0};
    ssize_t ret = ReadLine(sock , first_line , sizeof(first_line) - 1);
    if (ret < 0 ){
        if (ret == -2) {
            close(sock);
            return;
        }
        Handler404(sock);
        perror("ReadLine error\n");
        return;
    }
    fprintf(stderr , "ReadLine # %s\n" , first_line);
    ret = ParseFirstLine(&first , first_line);
    if (ret < 0) {
        Handler404(sock);
        perror("ParseFirstLine error\n");
        return;
    }
    
    Header header;
    InitHeader(&header);
    int total = 0;
    while(1) {
        char buf[SIZE] = {0};
        ReadLine(sock , buf , sizeof(buf) - 1);
        if (strcmp(buf , "\n") == 0) {
            break;
        }
        strcpy(header.header + total , buf);
        total += strlen(buf);
    }

    fprintf(stderr , "header # %s\n" , header.header);
    ret = ParseHeader(&first , &header);
    if (ret < 0) {
        Handler404(sock);
        perror("ParseHeader error\n");
        return;
    }

    HandlerResponse(sock , &first , &header);
    close(sock);
}

void* ThreadEntry(void* arg) {
    int64_t sock = (int64_t)arg;
    // 封装，为了代码的可维护性
    HandlerRequest(sock);
    return NULL;
}


//服务器的初始化，以及进入事件循环
void HttpServerInit(const char* ip , short port) {
    // 基本的初始化，基于 TCP
    int listen_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (listen_sock < 0) {
        perror("socket");
        return;
    }
    int opt = 1;
    setsockopt(listen_sock , SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt));
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    int ret = bind(listen_sock , (sockaddr*)&addr , sizeof(addr));
    if (ret < 0) {
        perror("bind");
        return;
    }
    ret = listen(listen_sock , 5);
    if (ret < 0) {
        perror("listen");
        return;
    }
    printf("HttpServer start OK\n");
    while (1) {
        sockaddr_in peer;
        socklen_t len = sizeof(peer);
        int64_t sock = accept(listen_sock , (sockaddr*)&peer , &len);
        if (sock < 0) {
            perror("accept");
            continue;
        }
        printf("------accept\n");
        pthread_t tid;
        pthread_create(&tid , NULL , ThreadEntry , (void*)sock);
        pthread_detach(tid);
    }
}

int main(int argc , char* argv[]) {
    if (argc != 3) {
        printf("Usage ./server.c [ip] [port]\n");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD , SIG_IGN);
    HttpServerInit(argv[1] , atoi(argv[2]));
    return 0;
}
