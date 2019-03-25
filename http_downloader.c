#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>

void parse_url(const char *url, char *host, int *port, char *file_name);   // 解析url
struct HTTP_RES_HEADER parse_header(const char *response);                 // HTTP协议头部解析
void get_ip_addr(char *host_name, char *ip_addr);                          // 获取ip地址
void progress_bar(long cur_size, long total_size, double time);            // 用于显示下载进度条
unsigned long get_file_size(const char *filename);                         // 获取文件大小
void download(int client_socket, char *file_name, long content_length);    // 下载文件函数

// 保持相应头信息
struct HTTP_RES_HEADER
{
    int status_code;          //HTTP/1.1 '200' OK
    char content_type[128];   //Content-Type: application/gzip
    long content_length;      //Content-Length: 11683079
};

// 解析url
void parse_url(const char *url, char *host, int *port, char *file_name)
{
    int j = 0;
    int start = 0;
    *port = 80;
    char *patterns[] = {"http://", "https://", NULL};

    // 分离下载地址中的http协议
    for (int i = 0; patterns[i]; i++) {
        if (strncmp(url, patterns[i], strlen(patterns[i])) == 0) {
            start = strlen(patterns[i]);
        }
    }

    // 解析域名, 这里处理时域名后面的端口号会保留
    for (int i = start; url[i] != '/' && url[i] != '\0'; i++, j++) {
        host[j] = url[i];
    }
    host[j] = '\0';

    // 解析端口号, 如果没有, 那么设置端口为80
    char *pos = strstr(host, ":");
    if (pos) {
        sscanf(pos, ":%d", port);
    }

    // 删除域名端口号
    for (int i = 0; i < (int)strlen(host); i++) {
        if (host[i] == ':') {
            host[i] = '\0';
            break;
        }
    }

    // 获取下载文件名
    j = 0;
    for (int i = start; url[i] != '\0'; i++) {
        if (url[i] == '/') {
           // if (i !=  strlen(url) - 1)
                j = 0;
            continue;
        }
        else {
            file_name[j++] = url[i];
        }
    }
    file_name[j] = '\0';
}

// HTTP协议头部解析
struct HTTP_RES_HEADER parse_header(const char *response)
{
    // 获取响应头的信息
    struct HTTP_RES_HEADER resp;

    // 获取返回代码
    char *pos = strstr(response, "HTTP/");
    if (pos) {
        sscanf(pos, "%*s %d", &resp.status_code);
    }

    // 获取返回文档类型
    pos = strstr(response, "Content-Type:");
    if (pos) {
        sscanf(pos, "%*s %s", resp.content_type);
    }

    // 获取返回文档长度
    pos = strstr(response, "Content-Length:");
    if (pos) {
        sscanf(pos, "%*s %ld", &resp.content_length);
    }

    return resp;
}

// 获取ip地址
void get_ip_addr(char *host_name, char *ip_addr)
{
    struct hostent *host = gethostbyname(host_name);
    if (!host) {
        ip_addr = NULL;
        return;
    }

    for (int i = 0; host->h_addr_list[i]; i++) {
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        break;
    }
}

// 用于显示下载进度条
void progress_bar(long cur_size, long total_size, double time)
{
    float percent = (float) cur_size / total_size;
    const int numTotal = 50;
    int numShow = (int)(numTotal * percent);

    if (numShow == 0) {
        numShow = 1;
    }

    if (numShow > numTotal) {
        numShow = numTotal;
    }

    char sign[51] = {0};
    memset(sign, '=', numTotal);

    printf("\r%.2f%%[%-*.*s] %.2f/%.2fMB %4.0fs", percent * 100, numTotal, numShow, 
        sign, cur_size / 1024.0 / 1024.0, total_size / 1024.0 / 1024.0, time);
    fflush(stdout);

    if (numShow == numTotal) {
        printf("\n");
    }
}

// 获取文件大小
unsigned long get_file_size(const char *filename)
{
    struct stat buf;
    if (stat(filename, &buf) < 0) {
        return 0;
    }
    return (unsigned long) buf.st_size;
}

// 下载文件函数
void download(int client_socket, char *file_name, long content_length)
{
   
    long hasrecieve = 0;              //记录已经下载的长度
    struct timeval t_start, t_end;    //记录一次读取的时间起点和终点, 计算速度
    int mem_size = 8192;              //缓冲区大小8K
    int buf_len = mem_size;           //理想状态每次读取8K大小的字节流
    int len;

    if (file_name[0] == '\0') {
        char *buf = (char *) malloc(mem_size * sizeof(char));
        int n = read(client_socket, buf, buf_len);
        buf[n] = '\0';
        printf("%s", buf);
    } else {
        int fd = open(file_name, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
        if (fd < 0) {
            printf("文件创建失败!\n");
            exit(0);
        }

        char *buf = (char *) malloc(mem_size * sizeof(char));

        //从套接字流中读取文件流
        long diff = 0;
        int prelen = 0;
        double speed;

        while (hasrecieve < content_length) {
            gettimeofday(&t_start, NULL );             // 获取开始时间
            len = read(client_socket, buf, buf_len);
            write(fd, buf, len);
            gettimeofday(&t_end, NULL );               // 获取结束时间

            hasrecieve += len;                         // 更新已经下载的长度

            // 计算速度
            if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0) {
                diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
            }

            progress_bar(hasrecieve, content_length, (diff*1.0/1000000)*1.16);

            if (hasrecieve == content_length) {
                break;
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    char url[2048] = "127.0.0.1";           //设置默认地址为本机,
    char host[64] = {0};                    //远程主机地址
    char ip_addr[16] = {0};                 //远程主机IP地址
    int port = 80;                          //远程主机端口, http默认80端口
    char file_name[256] = {0};              //下载文件名

    if (argc == 1) {
        printf("您必须给定一个http地址才能开始工作\n");
        exit(0);
    }
    else {
        strcpy(url, argv[1]);
    }

    parse_url(url, host, &port, file_name);    // 从url中分析出主机名, 端口号, 文件名

    if (argc == 3) {
        printf("\t您已经将下载文件名指定为: %s\n", argv[2]);
        strcpy(file_name, argv[2]);
    }

    get_ip_addr(host, ip_addr);               // 调用函数同访问DNS服务器获取远程主机的IP
    if (strlen(ip_addr) == 0) {
        printf("错误: 无法获取到远程服务器的IP地址, 请检查下载地址的有效性\n");
        return 0;
    }

    // 设置http请求头信息
    char header[2048] = {0};
    char hello[] = "/myhttpd.c";
    char location[1024];

    snprintf(location, sizeof(location), "/%s", file_name);
    sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
            "Connection: keep-alive\r\n"\
            "\r\n"\
            ,location, host);

    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0) {
        printf("套接字创建失败: %d\n", client_socket);
        exit(-1);
    }

    // 创建IP地址结构体
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(port);

    // 连接远程主机
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1) {
        printf("连接远程主机失败, error: %d\n", res);
        exit(-1);
    }

    write(client_socket, header, strlen(header));// write系统调用, 将请求header发送给服务器

    int mem_size = 4096;
    int length = 0;
    int len;
    char *buf = (char *) malloc(mem_size * sizeof(char));
    char *response = (char *) malloc(mem_size * sizeof(char));

    // 每次单个字符读取响应头信息
    puts("");
    while ((len = read(client_socket, buf, 1)) != 0) {
        if (length + len > mem_size) {
            // 动态内存申请, 因为无法确定响应头内容长度
            mem_size *= 2;
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL) {
                printf("动态内存申请失败\n");
                exit(-1);
            }
            response = temp;
        }

        buf[len] = '\0';
        strcat(response, buf);

        // 找到响应头的头部信息
        int flag = 0;
        for (int i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);

        // 连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
        if (flag == 4) {
            break;
        }   
            
        length += len;
    }

    struct HTTP_RES_HEADER resp = parse_header(response);

    if (resp.status_code != 200) {
        printf("文件无法下载, 远程主机返回: %d\n", resp.status_code);
        return 0;
    }

    if (file_name[0] != '\0') {
        printf("正在下载......\n");
    }

    download(client_socket, file_name, resp.content_length);

    if (file_name[0] == '\0') {
        printf("\n注：文件下载格式示例 ./a.out http://0.0.0.0:12345/3G测试视频.mp4\n\n");
    } 
    else if (resp.content_length == get_file_size(file_name)) {
        printf("\n文件%s下载成功! ^_^\n\n", file_name);
    } else {
        remove(file_name);
        printf("\n文件下载中有字节缺失, 下载失败, 请重试!\n\n");
    }

    shutdown(client_socket, 2);    // 关闭套接字的接收和发送
    return 0;
}
