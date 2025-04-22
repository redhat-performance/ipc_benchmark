#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>

typedef union {
    int val;
    struct semid_ds *buf;
    unsigned short *array ;
    struct seminfo *__buf;
    void   *__pad ;
} semun;

void sem_init(int sem_id, int sem_num, int init_valve)
{
    semun sem_union;
    sem_union.val = init_valve;
    if (semctl(sem_id, sem_num, SETVAL, sem_union))
    {
        perror("semctl");
        exit(-1);
    }
}

void sem_release(int sem_id, int sem_num)
{
    struct sembuf sem_b;
    sem_b.sem_num = sem_num;
    sem_b.sem_op = 1;
    sem_b.sem_flg = 0;
    if (semop(sem_id, &sem_b, 1) == -1)
    {
        perror("sem_release");
        exit(-1);
    }
}

void sem_reserve(int sem_id, int sem_num)
{
    struct sembuf sem_b;
    sem_b.sem_num = sem_num;
    sem_b.sem_op = -1;
    sem_b.sem_flg = 0;
    if (semop(sem_id, &sem_b, 1) == -1)
    {
        perror("sem_reserve");
        exit(-1);
    }
}

double getdetlatimeofday(struct timeval *begin, struct timeval *end)
{
    return (end->tv_sec + end->tv_usec * 1.0 / 1000000) -
           (begin->tv_sec + begin->tv_usec * 1.0 / 1000000);
}

double
time_diff(struct timeval x , struct timeval y)
{
    double x_ms , y_ms , diff;

    x_ms = (double)x.tv_sec*1000000 + (double)x.tv_usec;
    y_ms = (double)y.tv_sec*1000000 + (double)y.tv_usec;

    diff = (double)y_ms - (double)x_ms;
    return diff;
}

int compare_double( const void* a, const void* b )
{
    if( *(double*)a == *(double*)b ) return 0;
    return *(double*)a < *(double*)b ? -1 : 1;
}

int main(int argc, char const *argv[])
{
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define WRITE_SEM 0
#define READ_SEM 1

    pid_t pid;
    int sem_id;
    int shm_id;
    long count, size;
    int i;
    struct timeval begin, end;

    struct timeval tv_post_read;

    double time_elapsed;
    double avg_elapsed, tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies

    if (argc != 3)
    {
        printf("usage: ./shm <size> <count>\n");
        return 1;
    }

    size = atoi(argv[1]);
    count = atoi(argv[2]);

    double latencies[count];

    struct mybuf {
            int seq;
            struct timeval tv_time_sent;
            char *rest;
    } *buf;

    buf=malloc(size);
    
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    else if (pid == 0) // parent
    {
        sem_id = semget(SEM_KEY, 2, 0600 | IPC_CREAT);
        if (sem_id == -1)
        {
            perror("parent: semget");
            return -1;
        }
        sem_init(sem_id, WRITE_SEM, 1);
        sem_init(sem_id, READ_SEM, 0);

        shm_id = shmget(SHM_KEY, size, IPC_CREAT | 0600);
        if (shm_id == -1)
        {
            perror("parent: shmget");
            return -1;
        }
        void *addr = shmat(shm_id, NULL, 0);
        if (addr == (void *)-1)
        {
            perror("parent: shmat");
            return -1;
        }

        for (i = 0; i < count; i++)
        {
            sem_reserve(sem_id, READ_SEM);
            memcpy(buf, addr, size);
            // printf(">>>>>>>>%d\n", *(int*)buf);
            sem_release(sem_id, WRITE_SEM);
	    if (i == buf->seq) {
                gettimeofday(&tv_post_read, NULL);
                time_elapsed = time_diff(buf->tv_time_sent, tv_post_read);
                latencies[i]= time_elapsed;
                tot_elapsed+=time_elapsed;
            }
            else {
                    printf("buf seq= %d    %d\n", buf->seq, i);
                    printf("We did not match a sequence.\n");
                    exit(1);
            } 
	}
	avg_elapsed = tot_elapsed/count;
        ninetyninth=(count*0.99)-1;
        ninetyfifth=(count*0.95)-1;
        qsort( latencies, count, sizeof(double), compare_double );

        printf("%.0fusMIN\n", latencies[0]);
        printf("%.0fusAVG\n", avg_elapsed);
        printf("%.0fusMAX\n", latencies[count-1]);
        printf("%.0fus95th\n", latencies[ninetyfifth]);
        printf("%.0fus99th\n", latencies[ninetyninth]);
        

        if (shmdt(addr) == -1)
        {
            perror("child: shmdt");
            return -1;
        }
    }
    else // child
    {
        sleep(1);
        sem_id = semget(SEM_KEY, 0, 0);
        if (sem_id == -1)
        {
            perror("child: semget");
            return -1;
        }

        shm_id = shmget(SHM_KEY, 0, 0);
        if (shm_id == -1)
        {
            perror("child: shmget");
            return -1;
        }
        void *addr = (int *)shmat(shm_id, NULL, 0);
        if (addr == (void *)-1)
        {
            perror("child: shmat");
            return -1;
        }

        gettimeofday(&begin, NULL);

        for (i = 0; i < count; i++)
        {
	    buf->seq=i;
            gettimeofday(&buf->tv_time_sent, NULL);
            sem_reserve(sem_id, WRITE_SEM);
            // *(int*)buf = i;
            memcpy(addr, buf, size);
            sem_release(sem_id, READ_SEM);
        }

        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);
        printf("%.0fMB/s\n", count * size * 1.0 / (tm * 1024 * 1024));
        printf("%.0fmsg/s\n", count * 1.0 / tm);


        sem_reserve(sem_id, WRITE_SEM);
        semun dummy = {0};
        semctl(sem_id, 0, IPC_RMID, dummy);
        if (shmdt(addr) == -1)
        {
            perror("shmdt");
            return -1;
        }
        shmctl(shm_id, IPC_RMID, 0);
    }

    return 0;
}
