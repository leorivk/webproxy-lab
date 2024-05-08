#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000 // 최대 캐시 크기
#define MAX_OBJECT_SIZE 102400 // 최대 객체 크기

void doit(int fd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
  "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
  "Firefox/10.0.3\r\n";

// 로컬이 아닌 외부에서 테스트하기 위한 상수 (0 할당 시 도메인과 포트가 고정되어 외부에서 접속 가능)
const char *aws_ip = "107.22.130.196";

int main(int argc, char **argv) 
{
  int listenfd, clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // proxy listening socket
  listenfd = open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    clientfd = accept(listenfd, (SA *)&clientaddr, &clientlen); 
    getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    /* client의 요청을 받아서 end server로 forward */
    doit(clientfd);  
    close(clientfd);  
  }
}

void doit(int clientfd) 
{
  int serverfd;
  char request_buf[MAXLINE], response_buf[MAX_OBJECT_SIZE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  rio_t request_rio, response_rio;
  char *srcp;

  /* Request 1 - 요청 라인 읽기 [Client -> Proxy] */
  rio_readinitb(&request_rio, clientfd); 
  rio_readlineb(&request_rio, request_buf, MAXLINE);
  
  printf("Request headers : \n");
  printf("%s", request_buf);

  sscanf(request_buf, "%s %s", method, uri); // 요청 라인에서 method, uri 추출
  parse_uri(uri, hostname, port, path); // uri parsing -> hostname, port, path 추출
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0"); // end server에 전송하기 위해 요청 라인 수정
  
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD")) { 
    clienterror(clientfd, method, "501", "Not implemented", "Proxy does not implement this method");
    return;
  }

  printf("Hostname : %s, Port : %s\n", hostname, port);

  /* end server socket */
  serverfd = open_clientfd(hostname, port);
  if (serverfd < 0) {
    clienterror(clientfd, hostname, "404", "Not found", "Proxy couldn't connect to the server"); // 클라이언트에게 오류 응답 전송
    return;
  }

  printf("%s\n", request_buf); // 전송할 요청 헤더 출력

  /* Request 2 - 요청 라인 전송 [Proxy -> Server] */
  rio_writen(serverfd, request_buf, strlen(request_buf));

  /* Request 3 & 4 - 요청 헤더 읽기 & 전송 [Client -> Proxy -> Server] */
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);

  /* Response 1 - 응답 라인 읽기 & 전송 [Server -> Proxy -> Client] */
  rio_readinitb(&response_rio, serverfd);
  rio_readlineb(&response_rio, response_buf, MAXLINE);
  rio_writen(clientfd, response_buf, strlen(response_buf)); // 클라이언트에게 응답 라인 보내기

  /* Response 2 - 응답 헤더 읽기 & 전송 [Server -> Proxy -> Client] */
  int content_length;
  while (strcmp(response_buf, "\r\n"))
  {
      Rio_readlineb(&response_rio, response_buf, MAXLINE);
      if (strstr(response_buf, "Content-length")) // 응답 바디 수신에 사용하기 위해 바디 사이즈 저장
          content_length = atoi(strchr(response_buf, ':') + 1);
      Rio_writen(clientfd, response_buf, strlen(response_buf));
  }

  /* Response 3 - 응답 바디 읽기 & 전송 [Server -> Proxy -> Client] */
  if (content_length)
  {
      srcp = malloc(content_length);
      Rio_readnb(&response_rio, srcp, content_length);
      Rio_writen(clientfd, srcp, content_length);
      free(srcp);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  rio_writen(fd, buf, strlen(buf));
  rio_writen(fd, body, strlen(body));
}

void parse_uri(char *uri, char *hostname, char *port, char *path)
{ 
  char *hostnamep = strstr(uri, "//") != NULL ? strstr(uri, "//") + 2 : uri+1; // hostname 시작 위치 
  char *portp = strchr(hostnamep, ':'); // port 시작 위치 (없다면 NULL)
  char *pathp = strchr(hostnamep, '/'); // path 시작 위치 (없다면 NULL)

  if (portp) {
    strncpy(port, portp + 1, pathp - portp - 1); // port 구하기
    port[pathp - portp - 1] = '\0';
    strncpy(hostname, hostnamep, portp - hostnamep); // hostname 구하기
  } else { 
    strcpy(port, "8000"); 
    strncpy(hostname, hostnamep, pathp - hostnamep); // hostname 구하기
  }

  strcpy(path, pathp); // path 구하기
  return;

}

void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
    int is_host_exist = 0;
    int is_connection_exist = 0;
    int is_proxy_connection_exist = 0;
    int is_user_agent_exist = 0;

    Rio_readlineb(request_rio, request_buf, MAXLINE); // 요청 메시지의 첫번째 줄 읽기

    while (strcmp(request_buf, "\r\n")) // 버퍼에서 읽은 줄이 '\r\n'이 아닐 때까지 반복
    {
        if (strstr(request_buf, "Proxy-Connection") != NULL)
        {
            sprintf(request_buf, "Proxy-Connection: close\r\n");
            is_proxy_connection_exist = 1;
        }
        else if (strstr(request_buf, "Connection") != NULL)
        {
            sprintf(request_buf, "Connection: close\r\n");
            is_connection_exist = 1;
        }
        else if (strstr(request_buf, "User-Agent") != NULL)
        {
            sprintf(request_buf, user_agent_hdr);
            is_user_agent_exist = 1;
        }
        else if (strstr(request_buf, "Host") != NULL)
        {
            is_host_exist = 1;
        }
        Rio_writen(serverfd, request_buf, strlen(request_buf)); // Server에 전송
        Rio_readlineb(request_rio, request_buf, MAXLINE); // 다음 줄 읽기
    }

    // 필수 헤더 미포함 시 추가로 전송
    if (!is_proxy_connection_exist)
    {
        sprintf(request_buf, "Proxy-Connection: close\r\n");
        Rio_writen(serverfd, request_buf, strlen(request_buf));
    }
    if (!is_connection_exist)
    {
        sprintf(request_buf, "Connection: close\r\n");
        Rio_writen(serverfd, request_buf, strlen(request_buf));
    }
    if (!is_host_exist)
    {
      sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
      Rio_writen(serverfd, request_buf, strlen(request_buf));
    }
    if (!is_user_agent_exist)
    {
        sprintf(request_buf, user_agent_hdr);
        Rio_writen(serverfd, request_buf, strlen(request_buf));
    }

    // 요청 헤더 종료문 전송
    sprintf(request_buf, "\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
    return;
}