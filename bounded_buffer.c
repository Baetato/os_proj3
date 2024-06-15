#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#define N 8
#define MAX 1024
#define BUFSIZE 4
#define RUNTIME 130000    /* 출력량을 제한하기 위한 실행시간 (마이크로초) */
#define RED "\e[0;31m"
#define RESET "\e[0m"

atomic_int lock = 0;

/*
 * 스핀락 획득 함수
 */
void spin_lock(atomic_int *lock) {
    int expected = 0;
    while (!atomic_compare_exchange_weak(lock, &expected, 1)) {
        expected = 0; // 실패 시 expected를 다시 0으로 설정
        // 실패할 때까지 반복 (즉, lock이 0이 될 때까지 대기)
    }
}

/*
 * 생산자와 소비자가 공유할 버퍼를 만들고 필요한 변수를 초기화한다.
 */
int buffer[BUFSIZE]; //공유버퍼로 사용되는 배열.
                     // 해당 버퍼는 생산자가 아이템을 저장하고, 소비자가 아이템을 읽어가는 공간이다.
int in = 0; //버퍼에 아이템을 삽입할 위치를 나타내는 인덱스
int out = 0; //버퍼에 아이템을 제거할 위치를 나타내는 인덱스
int counter = 0; //버퍼에 현재 저장된 아이템의 수를 나타냄
int next_item = 0; //다음에 생산될 아이템의 번호
/*
 * 생산된 아이템과 소비된 아이템의 로그와 개수를 기록하기 위한 변수
 */
int task_log[MAX][2]; //생산된 아이템과 소비된 아이템의 로그를 기록하는 배열
int produced = 0; // 생산된 아이템의 총 개수
int consumed = 0; //소비된 아이템의 총 개수
/*
 * alive 값이 false가 될 때까지 스레드 내의 루프가 무한히 반복된다.
 */
bool alive = true; //스레드의 반복 실행을 제어하는 플래그

/*
 * 생산자 스레드로 실행할 함수이다. 아이템을 생성하여 버퍼에 넣는다.
 */
void *producer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        spin_lock(&lock);
        if(counter == BUFSIZE) {
            lock = 0;
            continue;
        }
        item = next_item++;
        buffer[in] = item;
        in = (in + 1) % BUFSIZE;
        counter++;
        
        if (task_log[item][0] == -1) {
            task_log[item][0] = i;
            produced++;
        } else {
            printf("<P%d,%d>....ERROR: 아이템 %d 중복생산\n", i, item, item);
            continue;
        }
        lock = 0;
        printf("<P%d,%d>\n", i, item);
    }
    pthread_exit(NULL);
}

/*
 * 소비자 스레드로 실행할 함수이다. 버퍼에서 아이템을 읽고 출력한다.
 */
void *consumer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        spin_lock(&lock);
        if(counter == 0) {
            lock = 0;
            continue;
        }
        item = buffer[out];
        out = (out + 1) % BUFSIZE;
        counter--;
        if (task_log[item][0] == -1) {
            printf("<C%d,%d>....ERROR: 아이템 %d 미생산\n", i, item, item);
            continue;
        } else if (task_log[item][1] == -1) {
            task_log[item][1] = i;
            consumed++;
        } else {
            printf("<C%d,%d>....ERROR: 아이템 %d 중복소비\n", i, item, item);
            continue;
        }
        lock = 0;
        printf("<C%d,%d>\n", i, item);
    }
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * 생산자와 소비자를 기록하기 위한 logs 배열을 초기화한다.
     */
    for (i = 0; i < MAX; ++i)
        task_log[i][0] = task_log[i][1] = -1;
    /*
     * N/2 개의 소비자 스레드를 생성한다.
     */
    for (i = 0; i < N/2; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, consumer, id+i);
    }
    /*
     * N/2 개의 생산자 스레드를 생성한다.
     */
    for (i = N/2; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, producer, id+i);
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
     * 생산된 아이템을 건너뛰지 않고 소비했는지 검증한다.
     */
    for (i = 0; i < consumed; ++i)
        if (task_log[i][1] == -1) {
            printf("....ERROR: 아이템 %d 미소비\n", i);
            return 1;
        }
    /*
     * 생산된 아이템의 개수와 소비된 아이템의 개수를 출력한다.
     */
    if (next_item == produced) {
        printf("Total %d items were produced.\n", produced);
        printf("Total %d items were consumed.\n", consumed);
    }
    else {
        printf("....ERROR: 생산량 불일치\n");
        return 1;
    }
    /*
     * 메인함수를 종료한다.
     */
    return 0;
}

