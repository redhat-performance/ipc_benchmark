#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

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

int main(int argc, char *argv[]) {
    int              fd;
    long             i, size, count, sum, n;
    /*char            *buf; */
    struct mybuf {
	    int seq;
	    struct timeval tv_time_sent;
	    char *rest;
    } *buf;
    struct timeval   begin, end;

    struct timeval tv_post_read;

    double time_elapsed;
    double avg_elapsed, tot_elapsed;

    int ninetyninth, ninetyfifth;  // 95th% and 99th% latencies
    	
    if (argc != 3) {
        printf("usage: ./fifo <size> <count>\n");
    	return 1;
    }

    size = atoi(argv[1]);
    count = atoi(argv[2]);

    double latencies[count];

    if (size < 32) {
	    printf("Specify a message size of at least 32 bytes\n");
	    exit(1);
    }


    buf = malloc(size);
    if (buf == NULL) {
        perror("malloc");
        return 1;
    }

    unlink("./fifo-ipc");
    if (mkfifo("./fifo-ipc", 0700) == -1) {
        perror("mkfifo");
        return 1;
    }

    fd = open("./fifo-ipc", O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (fork() == 0) {
        sum = 0;
        for (i = 0; i < count; i++) {
            n = read(fd, buf, size);
            if (n == -1) {
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
            sum += n;
        }

        if (sum != count * size) {
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
    
    } else {
        gettimeofday(&begin, NULL);

        for (i = 0; i < count; i++) {
		
	    buf->seq=i;
	    gettimeofday(&buf->tv_time_sent, NULL);
            if (write(fd, buf, size) != size) {
                perror("write");
                return 1;
            }
        }
        gettimeofday(&end, NULL);

        double tm = getdetlatimeofday(&begin, &end);
        printf("%.0fMB/s\n", count * size * 1.0 / (tm * 1024 * 1024));
        printf("%.0fmsg/s\n", count * 1.0 / tm);
    }

    unlink("./fifo-ipc");
    return 0;
}
