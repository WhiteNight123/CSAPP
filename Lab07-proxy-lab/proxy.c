#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE 10
#define SBUFSIZE 16
#define NTHREADS 4
sbuf_t sbuf; //连接缓冲区
             // uri结构
struct Uri{
  char host[MAXLINE]; // hostname
  char port[MAXLINE]; //端口
  char path[MAXLINE]; //路径
};
// Cache结构
typedef struct{
  char obj[MAX_OBJECT_SIZE];
  char uri[MAXLINE];
  int LRU;
  int isEmpty;

  int read_cnt; //读者数量
  sem_t w;      //保护Cache
  sem_t mutex;  //保护 read_cnt

} block;

typedef struct{
  block data[MAX_CACHE];
  int num;
} Cache;

Cache cache;

void doit(int connfd);
void parse_uri(char *uri, struct Uri *uri_data);
void build_header(char *http_header, struct Uri *uri_data, rio_t *client_rio);
void *thread(void *vargp);

void init_Cache();
int get_Cache(char *url);
int get_Index();
void update_LRU(int index);
void write_Cache(char *uri, char *buf);

/* 处理SIGPIPE信号 */
void sigpipe_handler(int sig){
  printf("haha?");
  return;
}

int main(int argc, char **argv){
  int listenfd, connfd;
  socklen_t clientlen;
  char hostname[MAXLINE], port[MAXLINE];

  struct sockaddr_storage clientaddr;
  init_Cache();
  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage :%s <port> \n", argv[0]);
    exit(1);
  }
  signal(SIGPIPE, sigpipe_handler);
  listenfd = Open_listenfd(argv[1]);

  sbuf_init(&sbuf, SBUFSIZE);
  //创建工作者线程
  for (int i = 0; i < NTHREADS; i++) {
    Pthread_create(&tid, NULL, thread, NULL);
  }

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    sbuf_insert(&sbuf, connfd);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s).\n", hostname, port);
  }
  return 0;
}
void *thread(void *vargp){
  Pthread_detach(pthread_self());
  while (1) {
    int connfd = sbuf_remove(&sbuf);
    doit(connfd);
    //关闭客户端的连接描述符
    Close(connfd);
  }
}
void doit(int connfd){
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char server[MAXLINE];

  rio_t rio, server_rio;

  char cache_tag[MAXLINE];
  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);
  strcpy(cache_tag, uri);

  if (strcasecmp(method, "GET")) {
    printf("Proxy does not implement the method");
    return;
  }

  struct Uri *uri_data = (struct Uri *)malloc(sizeof(struct Uri));
  //判断uri是否缓存，若缓存，直接回复
  int i;
  if ((i = get_Cache(cache_tag)) != -1) {
    //加锁
    P(&cache.data[i].mutex);
    cache.data[i].read_cnt++;
    if (cache.data[i].read_cnt == 1)
      P(&cache.data[i].w);
    V(&cache.data[i].mutex);

    Rio_writen(connfd, cache.data[i].obj, strlen(cache.data[i].obj));

    P(&cache.data[i].mutex);
    cache.data[i].read_cnt--;
    if (cache.data[i].read_cnt == 0)
      V(&cache.data[i].w);
    V(&cache.data[i].mutex);
    return;
  }

  //解析uri
  parse_uri(uri, uri_data);

  //设置header
  build_header(server, uri_data, &rio);

  //连接服务器
  int serverfd = Open_clientfd(uri_data->host, uri_data->port);
  if (serverfd < 0){
    printf("connection failed\n");
    return;
  }

  Rio_readinitb(&server_rio, serverfd);
  Rio_writen(serverfd, server, strlen(server));

  char cache_buf[MAX_OBJECT_SIZE];
  int size_buf = 0;
  size_t n;
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
    //注意判断是否会超出缓存大小
    size_buf += n;
    if(size_buf < MAX_OBJECT_SIZE)
      strcat(cache_buf, buf);
    printf("proxy received %d bytes,then send\n", (int)n);
    Rio_writen(connfd, buf, n);
  }
  Close(serverfd);

  if(size_buf < MAX_OBJECT_SIZE){
    write_Cache(cache_tag, cache_buf);
  }
}

void build_header(char *http_header, struct Uri *uri_data, rio_t *client_rio){
  char *User_Agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
  char *conn_hdr = "Connection: close\r\n";
  char *prox_hdr = "Proxy-Connection: close\r\n";
  char *host_hdr_format = "Host: %s\r\n";
  char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
  char *endof_hdr = "\r\n";

  char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
  sprintf(request_hdr, requestlint_hdr_format, uri_data->path);
  while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
    if (strcmp(buf, endof_hdr) == 0)
      break; /*EOF*/

    if (!strncasecmp(buf, "Host", strlen("Host")))  {
      strcpy(host_hdr, buf);
      continue;
    }

    if (!strncasecmp(buf, "Connection", strlen("Connection")) && !strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")) && !strncasecmp(buf, "User-Agent", strlen("User-Agent"))) {
      strcat(other_hdr, buf);
    }
  }
  if (strlen(host_hdr) == 0) {
    sprintf(host_hdr, host_hdr_format, uri_data->host);
  }
  sprintf(http_header, "%s%s%s%s%s%s%s",
      request_hdr,
      host_hdr,
      conn_hdr,
      prox_hdr,
      User_Agent,
      other_hdr,
      endof_hdr);

  return;
}

//解析uri
void parse_uri(char *uri, struct Uri *uri_data){
  char *hostpose = strstr(uri, "//");
  if (hostpose == NULL) {
    char *pathpose = strstr(uri, "/");
    if (pathpose != NULL)
      strcpy(uri_data->path, pathpose);
    strcpy(uri_data->port, "80");
    return;
  } else {
    char *portpose = strstr(hostpose + 2, ":");
    if (portpose != NULL) {
      int tmp;
      sscanf(portpose + 1, "%d%s", &tmp, uri_data->path);
      sprintf(uri_data->port, "%d", tmp);
      *portpose = '\0';
    } else {
      char *pathpose = strstr(hostpose + 2, "/");
      if (pathpose != NULL) {
        strcpy(uri_data->path, pathpose);
        strcpy(uri_data->port, "80");
        *pathpose = '\0';
      }
    }
    strcpy(uri_data->host, hostpose + 2);
  }
  return;
}
//初始化Cache
void init_Cache(){
  cache.num = 0;
  int i;
  for (i = 0; i < MAX_CACHE; i++) {
    cache.data[i].LRU = 0;
    cache.data[i].isEmpty = 1;
    // w, mutex均初始化为1
    Sem_init(&cache.data[i].w, 0, 1);
    Sem_init(&cache.data[i].mutex, 0, 1);
    cache.data[i].read_cnt = 0;
  }
}

//从Cache中找到内容
int get_Cache(char *url)
{
  int i;
  for (i = 0; i < MAX_CACHE; i++) {
    //读者加锁
    P(&cache.data[i].mutex);
    cache.data[i].read_cnt++;
    if (cache.data[i].read_cnt == 1)
      P(&cache.data[i].w);
    V(&cache.data[i].mutex);

    if ((cache.data[i].isEmpty == 0) && (strcmp(url, cache.data[i].uri) == 0))
      break;

    P(&cache.data[i].mutex);
    cache.data[i].read_cnt--;
    if (cache.data[i].read_cnt == 0)
      V(&cache.data[i].w);
    V(&cache.data[i].mutex);
  }
  if (i >= MAX_CACHE)
    return -1;
  return i;
}

//找到可以存放的缓存行
int get_Index(){
  int min = __INT_MAX__;
  int minindex = 0;
  int i;
  for (i = 0; i < MAX_CACHE; i++) {
    //读锁
    P(&cache.data[i].mutex);
    cache.data[i].read_cnt++;
    if (cache.data[i].read_cnt == 1)
      P(&cache.data[i].w);
    V(&cache.data[i].mutex);

    if (cache.data[i].isEmpty == 1) {
      minindex = i;
      P(&cache.data[i].mutex);
      cache.data[i].read_cnt--;
      if (cache.data[i].read_cnt == 0)
        V(&cache.data[i].w);
      V(&cache.data[i].mutex);
      break;
    }
    if (cache.data[i].LRU < min) {
      minindex = i;
      P(&cache.data[i].mutex);
      cache.data[i].read_cnt--;
      if (cache.data[i].read_cnt == 0)
        V(&cache.data[i].w);
      V(&cache.data[i].mutex);
      continue;
    }

    P(&cache.data[i].mutex);
    cache.data[i].read_cnt--;
    if (cache.data[i].read_cnt == 0)
      V(&cache.data[i].w);
    V(&cache.data[i].mutex);
  }

  return minindex;
}
//更新LRU
void update_LRU(int index){
  for (int i = 0; i < MAX_CACHE; i++) {

    if (cache.data[i].isEmpty == 0 && i != index) {
      P(&cache.data[i].w);
      cache.data[i].LRU--;
      V(&cache.data[i].w);
    }
  }
}
//写缓存
void write_Cache(char *uri, char *buf){

  int i = get_Index();
  //加写锁
  P(&cache.data[i].w);
  //写入内容
  strcpy(cache.data[i].obj, buf);
  strcpy(cache.data[i].uri, uri);
  cache.data[i].isEmpty = 0;
  cache.data[i].LRU = __INT_MAX__;
  update_LRU(i);

  V(&cache.data[i].w);
}
