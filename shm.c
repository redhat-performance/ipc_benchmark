#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#define SECONDS_PER_MINUTE 60 
#define SIZE_OF_USED_BUFFER 24    /* we put a sequence number and a timestamp at the beginning
                                  of the buffer */
#define  ARRAY_BOOST 20000        /* The size of the array initially and how much to increase it's
                                  size as it grows */  


/* Author:   Matt Currier
 *
 * This code base is from :
 * https://openbenchmarking.org/test/pts/ipc-benchmark ->
 * https://github.com/detailyang/ipc_benchmark
 *
 * MIT License
 * *
 * Copyright (c) 2017 detailyang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * To find out what cpus share a core (e.g. L1, L2 caches), on intel,
 * grep . /sys/devices/system/cpu/cpu*\/topology/thread_siblings_list
 * Stay away from a core-pair that has cpu0 in it.
 *
 */

int populate_buf(char *, int);
double getdetlatimeofday(struct timeval *, struct timeval *);
double time_diff(struct timeval , struct timeval );


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
        perror("semop in sem_release");
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
        perror("semop in sem_reserve");
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

/* sort the array of doubles (latencies) */
int compare_double( const void* a, const void* b )
{
    if( *(double*)a == *(double*)b ) return 0;
    return *(double*)a < *(double*)b ? -1 : 1;
}

/* create a buffer of random lower case letters */
int populate_buf(char *buffer, int length) {

    int i = length - 1;
    int r = rand() % 26;

    buffer[i] = '\0';

    while ( SIZE_OF_USED_BUFFER != i) {
        i = i - 1;
        buffer[i] = 'a' + r;
        r++;
        if (r==26)
           r = 0;
    }
    return 0;
}


/* ================================================================================================================================= */

int main(int argc, char const *argv[])
{
#define SHM_KEY1 0x1234
#define SEM_KEY1 0x5678
#define SHM_KEY2 0x1239
#define SEM_KEY2 0x5679
#define WRITE_SEM 0
#define READ_SEM 1

    pid_t pid;
    int sem_id1;
    int shm_id1;
    int sem_id2;
    int shm_id2;

    long msgtot, size;
    struct timeval begin, end;
    short mins;
    long ctr;
    short runnum;

    char latencies_file[50];
    FILE *outfile_latencies;

    time_t start;

    struct timeval tv_post_read;

    double time_elapsed;
    double avg_elapsed;
    double tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies

    if (argc != 4)
    {
        printf("usage: ./shm <message size> <mins_to_run> <run_number> \n");
        return 1;
    }
   
    typedef struct  {
            double start_time;
            double end_time;
    } latencies_arr;


    size = atoi(argv[1]);
    mins = atoi(argv[2]);
    runnum = atoi(argv[3]);

    int new_size = ARRAY_BOOST;
    
    double *latencies = (double*) malloc(ARRAY_BOOST * sizeof(double));
    if (latencies == NULL) {
        printf("Latencies memory not allocated.\n");
        return 1;
    }
    latencies_arr *lat = (latencies_arr*) malloc(ARRAY_BOOST * sizeof(latencies_arr));
    if (lat == NULL) {
        printf("Latencies array memory not allocated.\n");
        return 1;
    }

    snprintf(latencies_file,50,"shm_latencies_%d_%hi.json", (int) size, runnum);

    outfile_latencies = fopen(latencies_file,"w");

    struct mybuf {
            int seq;
            struct timeval tv_time_sent;
            char *rest;
    } *buf;

    buf=malloc(size);
    if (buf == NULL) {
        perror("malloc");
        return 1;
    }
    
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }
    else if (pid == 0) // parent
    {
        sem_id1 = semget(SEM_KEY1, 2, 0600 | IPC_CREAT);
        if (sem_id1 == -1)
        {
            perror("parent: semget 1");
            return -1;
        }
        sem_init(sem_id1, WRITE_SEM, 1);
        sem_init(sem_id1, READ_SEM, 0);

        shm_id1 = shmget(SHM_KEY1, size, IPC_CREAT | 0600);
        if (shm_id1 == -1)
        {
            perror("parent: shmget 1");
            return -1;
        }
        void *addr1 = shmat(shm_id1, NULL, 0);
        if (addr1 == (void *)-1)
        {
            perror("parent: shmat 1");
            return -1;
        }

    	sem_id2 = semget(SEM_KEY2, 2, 0600 | IPC_CREAT);
        if (sem_id2 == -1)
        {
            perror("parent: semget 2");
            return -1;
        }
        sem_init(sem_id2, WRITE_SEM, 1);
        sem_init(sem_id2, READ_SEM, 0);

        shm_id2 = shmget(SHM_KEY2, size, IPC_CREAT | 0600);
        if (shm_id2 == -1)
        {
            perror("parent: shmget 2");
            return -1;
        }
        void *addr2 = shmat(shm_id2, NULL, 0);
        if (addr2 == (void *)-1)
        {
            perror("parent: shmat 2");
            return -1;
        }

        msgtot=0;

        start = time(NULL);
        while (time(NULL) - start < (time_t) (( mins * SECONDS_PER_MINUTE) + 1.0))  {    // run for "mins" minutes
            sem_reserve(sem_id1, READ_SEM);
            memcpy(buf, addr1, size);          // receive message 1st time
            sem_release(sem_id1, WRITE_SEM);
	        if (msgtot == buf->seq) {          // ensure correct ordering		
                 sem_reserve(sem_id2, WRITE_SEM);
                 memcpy(addr2, buf, size);     // send message back to parent
                 sem_release(sem_id2, READ_SEM);
            }
            else {
                 printf("xxxxxxbuf seq= %d    %ld\n", buf->seq, msgtot);
                 printf("We did not match a sequence going to.\n");
                 exit(-1);
            } 
	        msgtot++;
        }

        if (shmdt(addr1) == -1)
        {
            perror("parent: shmdt 1");
            return -1;
        }
        if (shmdt(addr2) == -1)
        {
            perror("parent: shmdt 2");
            return -1;
        }
    }
    else // child
    {
        sleep(1);
        sem_id1 = semget(SEM_KEY1, 0, 0);
        if (sem_id1 == -1)
        {
            perror("child: semget 1");
            return -1;
        }

        shm_id1 = shmget(SHM_KEY1, 0, 0);
        if (shm_id1 == -1)
        {
            perror("child: shmget 1");
            return -1;
        }
        void *addr1 = (int *)shmat(shm_id1, NULL, 0);
        if (addr1 == (void *)-1)
        {
            perror("child: shmat 1");
            return -1;
        }

        sem_id2= semget(SEM_KEY2, 0, 0);
        if (sem_id2 == -1)
        {
            perror("child: semget 2");
            return -1;
        }

        shm_id2 = shmget(SHM_KEY2, 0, 0);
        if (shm_id2 == -1)
        {
            perror("child: shmget 2");
            return -1;
        }
        void *addr2 = (int *)shmat(shm_id2, NULL, 0);
        if (addr2 == (void *)-1)
        {
            perror("child: shmat 2");
            return -1;
        }

        gettimeofday(&begin, NULL);

        start = time(NULL);

        msgtot=0;
        while (time(NULL) - start < (time_t) ( mins * SECONDS_PER_MINUTE))  {
            buf->seq=msgtot;
            gettimeofday(&buf->tv_time_sent, NULL);  // timestamp before initial message sent
            populate_buf((char*)buf, size);          // populate the message buf
            // printf("buf1=%s\n", (char*) buf+SIZE_OF_USED_BUFFER);
            sem_reserve(sem_id1, WRITE_SEM);
            memcpy(addr1, buf, size);                // send message 1st time to parent
            sem_release(sem_id1, READ_SEM);
            sem_reserve(sem_id2, READ_SEM);
            memcpy(buf, addr2, size);                // receive message back from parent
            sem_release(sem_id2, WRITE_SEM);
            // printf("buf2=%s\n", (char*) buf+SIZE_OF_USED_BUFFER);
            if  (msgtot == new_size)  {
                new_size += ARRAY_BOOST;
                        double *latencies_tmp = realloc(latencies, new_size * sizeof(double)); 
                if (latencies_tmp == NULL)  {
                printf("Size increase of latencies array failed.\n");
                        return 1;
                }
    
                latencies_arr *lat_tmp = realloc(lat, new_size * sizeof(latencies_arr));
                if (lat_tmp == NULL)  {
                printf("Size increase of lat structure array failed.\n");
                        return 1;
                }
                latencies = latencies_tmp;
                lat = lat_tmp;
             }
             if (msgtot == buf->seq) {                // ensure correct ordering
                gettimeofday(&tv_post_read, NULL);   // capture round trip timestamp
                time_elapsed = time_diff(buf->tv_time_sent, tv_post_read);    // get latency
                lat[msgtot].start_time = ((double) buf->tv_time_sent.tv_sec*1000000 + (double)buf->tv_time_sent.tv_usec);
                latencies[msgtot] = time_elapsed;
                lat[msgtot].end_time = ((double) tv_post_read.tv_sec*1000000 + (double)tv_post_read.tv_usec);
                tot_elapsed += time_elapsed;
            }
            else {
                    printf("buf seq= %d    %ld\n", buf->seq, msgtot);
                    printf("We did not match a sequence coming back.\n");
                    exit(1);
            } 
	        msgtot++;
        }

        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);

        printf("%.0fMB/s\n", msgtot * size * 1.0 / (tm * 1024 * 1024));
        printf("%.0fmsg/s\n", msgtot * 1.0 / tm);

        avg_elapsed = tot_elapsed/msgtot;
        ninetyninth = (msgtot*0.99)-1;
        ninetyfifth = (msgtot*0.95)-1;

        fprintf(outfile_latencies, "{\n");            // write out latency data
        for (ctr = 0; ctr < msgtot-1; ctr++) {
            fprintf(outfile_latencies, "  {\"start-time\": %.0f, \"latency\": %.0f, \"end-time\": %.0f},\n ", lat[ctr].start_time, latencies[ctr], lat[ctr].end_time);
        } 
        fprintf(outfile_latencies, "  {\"start-time\": %.0f, \"latency\": %.0f, \"end-time\": %.0f}\n}", lat[msgtot-1].start_time, latencies[msgtot-1], lat[msgtot-1].end_time);

        qsort( latencies, msgtot, sizeof(double), compare_double );

        printf("%.0fusMIN\n", latencies[0]);
        printf("%0.2fusAVG\n", avg_elapsed);
        printf("%.0fusMAX\n", latencies[msgtot-1]);
        printf("%.0fus95th\n", latencies[ninetyfifth]);
        printf("%.0fus99th\n", latencies[ninetyninth]);

        printf("The total messages sent to and from the parent: %ld\n", msgtot);

        sem_reserve(sem_id1, WRITE_SEM);
        semun dummy = {0};
        semctl(sem_id1, 0, IPC_RMID, dummy);
        if (shmdt(addr1) == -1)
        {
            perror("shmdt 1");
            return -1;
        }
        shmctl(shm_id1, IPC_RMID, 0);

        sem_reserve(sem_id2, WRITE_SEM);
        semctl(sem_id2, 0, IPC_RMID, dummy);
        if (shmdt(addr2) == -1)
        {
            perror("shmdt 2");
            return -1;
        }
        shmctl(shm_id2, IPC_RMID, 0);
        
   }
    free(latencies);
    free(lat);
    fclose(outfile_latencies);

    return 0;
}
