#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd; // 소켓 식별자
    char *host, *port, buf[MAXLINE], tmp[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage : %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];

    
    // host에서 돌아가고 port에 연결 요청을 듣는 서버와 연결 설정 후 열린 소켓 식별자 반환
    clientfd = Open_clientfd(host, port);

    // 소켓 식별자 clientfd를 주소 rio에 위치한 rio_t 타입의 읽기 버퍼와 연결 
    Rio_readinitb(&rio, clientfd);

    // Fgets가 EOF 표준 입력 만나면 종료
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        // buf에 저장된 데이터를 클라이언트 소켓에 쓰기
        Rio_writen(clientfd, buf, strlen(buf));
        // 서버로부터 응답 buf을 읽어들여 rio에 저장
        Rio_readlineb(&rio, tmp, MAXLINE);
        Fputs(buf, stdout);
    }

    Close(clientfd);
    exit(0);

}



    /*

    Fgets
        Fgets(buf, MAXLINE, stdin) : 표준 입력에서 데이터를 한 줄씩 읽어들입니다. 
        
        각 인자의 역할

        - buf
            읽어들인 데이터를 저장할 버퍼입니다. 이 버퍼에 읽은 데이터가 저장됩니다.

        - MAXLINE
            버퍼의 최대 크기입니다. 읽어들일 수 있는 최대 문자 수를 나타냅니다. 
            버퍼 크기를 넘어가는 데이터는 잘립니다.

        - stdin
            데이터를 읽어들일 스트림입니다. 
            표준 입력 스트림(stdin)에서 데이터를 읽어들이기 위해 사용됩니다.

        Fgets 함수는 표준 입력 스트림에서 한 줄씩 데이터를 읽어들이며, 
        개행 문자(\n)가 나타날 때까지 또는 버퍼 크기 제한에 도달할 때까지 읽습니다.
        그리고 읽은 데이터를 buf에 저장합니다.

        만약 표준 입력 스트림에서 더 이상 읽어들일 데이터가 없거나 
        에러가 발생하면(EOF에 도달하거나 에러가 발생하면),
        Fgets 함수는 NULL을 반환합니다.

        예를 들어, 사용자가 키보드로 "Hello, world!\n"를 입력했다면, 
        buf에는 "Hello, world!\n"가 저장될 것입니다.
        만약 버퍼 크기를 초과하는 데이터가 입력되면 초과하는 부분은 잘리고 버퍼 크기에 맞게 저장됩니다.

    Fputs
        표준 출력 스트림에 문자열을 출력하는 함수입니다. 
        
        각 인자의 역할

        - buf
            출력할 문자열이 저장된 버퍼입니다.
        - stdout
            출력할 스트림입니다. 
            표준 출력 스트림(stdout)으로 설정하여, 문자열을 화면에 출력합니다.

        puts 함수는 buf에 저장된 문자열을 표준 출력으로 보냅니다. 
        이 함수는 문자열의 끝에 널 종료 문자(\0)가 있을 때까지 문자열을 출력합니다.

        예를 들어, buf에 "Hello, world!\n"가 저장되어 있다면, 
        이 함수는 "Hello, world!\n"을 표준 출력에 출력합니다.
        문자열의 끝에 널 종료 문자가 있으므로, 문자열의 끝까지 출력됩니다.

    */
