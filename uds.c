#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

double
getdetlatimeofday(struct timeval *begin, struct timeval *end)
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

int main(int argc, char *argv[])
{
    int fd, nfd;
    long size, count, sum, n;
    int  i;
    size_t len;
    struct timeval begin, end;
    struct sockaddr_un un;

     struct mybuf {
            int seq;
            struct timeval tv_time_sent;
            char *rest;
    } *buf;

    struct timeval tv_post_read;
    double time_elapsed;
    double avg_elapsed, tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies

    if (argc != 3)
    {
        printf("usage: ./uds <size> <count>\n");
        return 1;
    }

    size = atoi(argv[1]);
    count = atoi(argv[2]);
    buf = malloc(size);
    double latencies[count];
    if (size < 32) {
            printf("Specify a message size of at least 32 bytes\n");
            exit(1);
    }


    memset(&un, 0, sizeof(un));
    if (fork() == 0)
    {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        unlink("./uds-ipc");
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, "./uds-ipc");
        len = offsetof(struct sockaddr_un, sun_path) + strlen("./uds-ipc");

        if (bind(fd, (struct sockaddr *)&un, len) == -1)
        {
            perror("bind");
            return 1;
        }

        if (listen(fd, 128) == -1)
        {
            perror("listen");
            return 1;
        }

        if ((nfd = accept(fd, NULL, NULL)) == -1)
        {
            perror("accept");
            return 1;
        }
        sum = 0;
	i=0;
        for (;;)
        {
            n = read(nfd, buf, size);
            if (n == 0)
            {
                break;
            }
            else if (n == -1)
            {
                perror("read");
                return 1;
            }
	    if (i == buf->seq) {
                gettimeofday(&tv_post_read, NULL);
                time_elapsed = time_diff(buf->tv_time_sent, tv_post_read);
                latencies[i]= time_elapsed;
                tot_elapsed+=time_elapsed;
            }
            else {
                    printf("We did not match a sequence.\n");
                    exit(1);
            }
            i++;
            sum += n;
        }

        if (sum != count * size)
        {
            fprintf(stderr, "sum error: %ld != %ld\n", sum, count * size);
            return 1;
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
    }
    else
    {
        sleep(1);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        un.sun_family = AF_UNIX;
        strcpy(un.sun_path, "./uds-ipc");
        len = offsetof(struct sockaddr_un, sun_path) + strlen("./uds-ipc");
        if (connect(fd, (struct sockaddr *)&un, len) == -1)
        {
            perror("connect");
            return 1;
        }

        gettimeofday(&begin, NULL);

        for (i = 0; i < count; i++)
        {
	    buf->seq=i;
            gettimeofday(&buf->tv_time_sent, NULL);
            if (write(fd, buf, size) != size)
            {
                perror("write");
                return 1;
            }
        }

        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);
	printf("%.0fMB/s\n", count * size * 1.0 / (tm * 1024 * 1024));
        printf("%.0fmsg/s\n", count * 1.0 / tm);

    }

    unlink("./uds-ipc");
    return 0;
}
