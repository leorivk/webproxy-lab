#include <stdio.h>

#include "csapp.h"
#include "cache.h"

web_object_t *rootp;
web_object_t *lastp;
int total_cache_size = 0;

// 캐싱된 웹 객체 중 해당 path를 가진 객체 반환
web_object_t*find_cache(char *path)
{
    if (!rootp) return NULL;
    web_object_t *cur = rootp; // 루트 객체부터 탐색 시작
    while (cur) {
        if (!strcmp(cur->path, path)) return cur;
        cur = cur->next;
    }
    return NULL;
}

// 웹 객체(web_object)에 저장된 응답을 Client에 전송
void send_cache(web_object_t *web_object, int clientfd)
{
    /* 1. Response Header 생성, 전송 */
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 상태 코드
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); // 서버 이름
    sprintf(buf, "%sConnection: close\r\n", buf); // 연결 방식
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, web_object->content_length); // 컨텐츠 길이
    Rio_writen(clientfd, buf, strlen(buf)); // client에게 전송

    /* 2. 캐싱된 Response Body 전송 */
    Rio_writen(clientfd, web_object->response_ptr, web_object->content_length);
}

// 사용한 웹 객체(web_object)를 캐시 연결 리스트의 root로 갱신
void new_cache(web_object_t *web_object)
{
    // Case 1. 현재 객체가 이미 Root인 경우
    if (web_object == rootp) return;

    // Case 2. 이전 객체, 다음 객체가 이미 존재하는 경우
    if (web_object->prev && web_object->next) {
        web_object_t *prevobj = web_object->prev;
        web_object_t *nextobj = web_object->next;

        web_object->prev->next = nextobj;
        web_object->next->prev = prevobj;
        
    } else // Case 3. 다음 객체가 없는 경우 (현재 객체가 마지막인 경우)
        web_object->prev->next = NULL; 

    // 현재 객체를 Root로 변경
    web_object->next = rootp; // Root였던 객체는 현재 객체의 다음 객체로 갱신
    rootp = web_object; // 현재 객체를 Root로 갱신

}

// 인자로 전달된 웹 객체(web_object)를 캐시 연결 리스트에 추가
void write_cache(web_object_t *web_object)
{
    total_cache_size += web_object->content_length;

    // 최대 총 캐시 크기를 초과한 경우 사용한지 가장 오래된 객체부터 제거
    while (total_cache_size > MAX_CACHE_SIZE) {
        total_cache_size -= lastp->content_length; // 마지막 객체 크기만큼 최대 총 캐시 크기 감소
        lastp = lastp->prev; // 마지막 객체를 이전 객체로 변경
        free(lastp->next); // 제거한 객체의 메모리 반환
        lastp->next = NULL;
    }

    // 캐시 연결리스트가 비어있다면,
    if (!rootp) lastp = web_object;
    // 비어있지 않다면,
    else {
        web_object->next = rootp;
        rootp->prev = web_object;
    }
    rootp = web_object;

}