/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    /**
     * 클라이언트로부터의 연결 요청이 듣기 소켓 listenfd에 도달하기를 기다리고,
     * 클라이언트와 통신하기 위해 사용될 수 있는 연결 식별자 반환
     * 
     * 클라이언트의 소켓 주소 : &clientaddr
     * 
    */
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // 연결 요청 올때까지 대기

    /**
     * 소켓 주소 구조체 (SA *)&clientaddr를 대응되는 호스트와 서비스 이름 스트링으로 변환,
     * 이들을 hostname, port 버퍼로 복사
    */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/* 한 개의 HTTP 트랜잭션 처리 */
void doit(int fd) 
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio; // 빈 버퍼 설정

  /* Read request line and headers */

  /**
   * 빈 버퍼 rio와 파일 디스크립터 fd 연결
   * 여기서 fd는 connfd가 전달됨
   **/
  Rio_readinitb(&rio, fd); 
  /**
   * 다음 텍스트 줄을 파일 rio(종료 새 줄 문자를 포함해서)에서 읽고,
   * 이것을 메모리 위치 buf로 복사하고, 텍스트 라인얼 널(0) 문자로 종료
   * 최대 MAXLINE-1 바이트를 읽으며, 종료욜 널 문자를 위한 공간을 남겨둔다.
   * */
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers : \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); 
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD")) { // GET이 아니면
    clienterror(fd, method, "501", "Not implemented",
            "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn’t find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    // 보통 파일이거나 읽기 권한을 가지고 있다면 정적 컨텐츠 클라이언트에게 제공
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { 
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn’t read the file");
        return;
    }
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else { /* Serve dynamic content */
    // 보통 파일이거나 읽기 권한을 가지고 있다면 동적 컨텐츠 클라이언트에게 제공
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn’t run the CGI program");
        return; 
    }
    serve_dynamic(fd, filename, cgiargs);
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
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) { /* 빈 줄*/
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) { /* Static content : uri에 cgi-bin 미포함*/
    strcpy(cgiargs, ""); // cgiargs를 빈 문자열로
    strcpy(filename, "."); // filename을 .로
    strcat(filename, uri); // filename + uri
    if (uri[strlen(uri) - 1] == '/') strcat(filename, "home.html");
    return 1;
  } else { /* Dynamic content */
    ptr = index(uri, '?'); // ?가 처음으로 나타나는 위치
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    } else strcpy(cgiargs, "");

    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/* Homework 11.11 
* method 인자 추가
*/
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 요청된 파일의 MIME 타입 결정
  // 각 헤더를 문자열 버퍼 buf에 추가
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // 생성된 헤더를 클라이언트에게 전송
  printf("Response headers:\n");
  printf("%s", buf);

  /* Homework 11.11 
  * HEAD 메서드일 시 여기서 반환 (헤더 까지)
  */
  if (strcasecmp(method, "HEAD") == 0)
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // 요청된 파일을 읽기 전용으로 Open
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 매핑 (srcp 포인터에 매핑)
  /* Homework 11.9 */
  srcp = (char *)malloc(filesize);
  rio_readn(srcfd, srcp, filesize);
  Close(srcfd); // 파일을 읽은 후 파일 디스크립터 Close
  Rio_writen(fd, srcp, filesize); // 메모리 매핑된 파일을 클라이언트에게 전송
  // Munmap(srcp, filesize); // 메모리 매핑 해제
  free(srcp);

}

/**
 * get_filetype - Derive file type from filename
*/
void get_filetype(char *filename, char *filetype)
{
  /* filename이 어떤 확장자 명을 포함하는지에 따라 filetype 결정 */
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  /* Homework 11.7 */
  else if (strstr(filename, ".mp4")) 
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}


void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  if (Fork() == 0) { /* Child */
      /* Real server would set all CGI vars here */
      setenv("QUERY_STRING", cgiargs, 1);
      Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
      Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}