#include "csapp.h"

void echo(int connfd);
void print_client_address(const struct sockaddr_storage *clientaddr);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0);
    }

    // 서버에 ./echoserver 2999 입력 -> argv[1] : 2999

    // 듣기 소켓
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        // 연결 소켓
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        /*
        소켓 주소 구조체 clientaddr를
        client_hostname(호스트), client_port(서비스) 이름 스트링으로 변환 후,
        이들을 각각의 버퍼로 복사
        */ 
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);

        // 통신(클라이언트가 요청 -> 서버가 응답)이 끝날 때마다 연결 소켓 닫기
        Close(connfd);
    }

    exit(0);

}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {

        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}

void print_client_address(const struct sockaddr_storage *clientaddr) {
    struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)clientaddr;
    char ip_str[INET_ADDRSTRLEN];

    // IP 주소를 문자열로 변환
    inet_ntop(AF_INET, &(ipv4_addr->sin_addr), ip_str, INET_ADDRSTRLEN);

    // 포트 번호를 호스트 바이트 순서에서 네트워크 바이트 순서로 변환
    int port = ntohs(ipv4_addr->sin_port);

    printf("Client IP: %s\n", ip_str);
    printf("Client Port: %d\n", port);
}