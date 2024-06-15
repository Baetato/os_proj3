#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h> // C11 표준의 Atomic 연산을 위해 추가

#define N 8             /* 스레드 개수 */
#define RUNTIME 100000  /* 출력량을 제한하기 위한 실행시간 (마이크로초) */

/*
 * ANSI 컬러 코드: 출력을 쉽게 구분하기 위해서 사용한다.
 * 순서대로 BLK, RED, GRN, YEL, BLU, MAG, CYN, WHT, RESET을 의미한다.
 */
char *color[N+1] = {"\e[0;30m","\e[0;31m","\e[0;32m","\e[0;33m","\e[0;34m","\e[0;35m","\e[0;36m","\e[0;37m","\e[0m"};

/*
 * waiting[i]는 스레드 i가 임계구역에 들어가기 위해 기다리고 있음을 나타낸다.
 * alive 값이 false가 될 때까지 스레드 내의 루프가 무한히 반복된다.
 */
bool waiting[8]={0};
bool alive = true;
atomic_bool lock = false;

/*
 * N 개의 스레드가 임계구역에 배타적으로 들어가기 위해 스핀락을 사용하여 동기화한다.
 */
void *worker(void *arg)
{
    int i = *(int *)arg; //스레드 별로 고유 번호를 받음
    
    while (alive) { //alive 변수가 false가 될 때까지 계속 실행
        /*
         * 임계구역: 알파벳 문자를 한 줄에 40개씩 10줄 출력한다.
         */
        bool expected = false;
        // atomic_compare_exchange_weak 함수를 사용하여 lock 변수를 안전하게 true로 설정
        // 다른 스레드가 lock을 이미 true로 설정했다면, 이 스레드는 계속해서 대기
        while(!atomic_compare_exchange_weak(&lock, &expected, true)){
            expected = false; //실패했을 경우 expected를 다시 false로 설정
        }
        // 스레드가 아직 임계구역을 수행하지 않았다면 해당 로직 수행
        if(!(waiting[i])){
            for (int k = 0; k < 400; ++k) {
                printf("%s%c%s", color[i], 'A'+i, color[N]);
                if ((k+1) % 40 == 0)
                    printf("\n");
            }
            /*
             * 임계구역이 성공적으로 종료되었다.
             */
            waiting[i] = 1; //해당 스레드가 임계구역을 수행했음을 표시
            lock = false; // lock을 해제하여 다른 스레드가 임계구역에 접근할 수 있도록 한다
            break;
        }
        lock = false;
    }
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * N 개의 자식 스레드를 생성한다.
     */
    for (i = 0; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, worker, id+i);
    }
    /*
     * 스레드가 출력하는 동안 RUNTIME 마이크로초 쉰다.
     * 이 시간으로 스레드의 출력량을 조절한다.
     */
    usleep(RUNTIME);
    /*
     * 스레드가 자연스럽게 무한 루프를 빠져나올 수 있게 한다.
     */
    alive = false;
    /*
     * 자식 스레드가 종료될 때까지 기다린다.
     */
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
    /*
     * 메인함수를 종료한다.
     */
    return 0;
}

