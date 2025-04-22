#! /bin/bash

# Tests file binaries and test sizes
#

ulimit -s unlimited

ipc_tests="shm fifo"

udp_sizes="128 256 512 1024 2048 4096"
uds_sizes="128 256 512 1024 2048 4096 8192 32768"
socketpair_sizes="128 256 512 1024 2048 4096 8192 32768"
ipc_sizes="128 256 512 1024 2048 4096 8192 32768 65536"
ipc_sizes_full="128 256 512 1024 2048 4096 8192 32768 65536"
mins="1"

ipc_count=20000000
udp_count=20000000
tcp_count=20700000
pipe_count=20000000
shm_count=20000000
fifo_count=20010000
udp_count=20000000
uds_count=22030000
shm_count=21000000
posixq_count=21020000

# Write to log file, keeps echo parameters
write_log()
{
    local arg=""

    if [ $# -eq 7 ]; then
	arg=$1
	shift
    fi
    echo ${arg} $1 >> ${logfile}
}

# Check whether the test binaries actually exist
for test in ${ipc_tests}
do
    if [ ! -x ${test} ]; then
	echo "Program ${test} do not exist or is not a binary."
	echo "Have you built the tests?"
	exit 1
    fi
done

logfile=$(mktemp /tmp/ipc.XXXXXX)
write_log "The IPC default count value is:  ${ipc_count}"

for iter in 1 2 3 4 5
do
# Call the test
  for test in ${ipc_tests}
  do
    # Initialize
    case $test in
	    "udp")
                ipc_sizes=${udp_sizes}
                ipc_count=${udp_count}
		;;
	    "uds")
                ipc_sizes=${uds_sizes}
                ipc_count=${uds_count}
		;;
            "socketpair")
                ipc_sizes=${socketpair_sizes}
                ipc_count=${ipc_count}
		;;
	    "tcp")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${tcp_count}
		;;
            "pipe")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${pipe_count}
		    ;;
	    "fifo")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${fifo_count}
		    ;;
	    "fifo-2way-1stamp")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${fifo_count}
		    ;;
	    "shm")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${shm_count}
		    ;;
	    "shm-2way-1stamp")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${shm_count}
		    ;;
	    "uds")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${uds_count}
		    ;;
	    "posixq")
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${posixq_count}
		    ;;
	    *)
                ipc_sizes=${ipc_sizes_full}
                ipc_count=${ipc_count}
		    ;;
    esac

    line0=""
    line1=""
    line2=""
    line3=""
    line4=""
    line5=""
    line6=""
    line7=""

    write_log "${test} ${ipc_count}" 

    for tsize in ${ipc_sizes}
    do
	    ./${test} ${tsize} ${ipc_count} ${mins} ${iter} > results.txt
	    line0="${line0}|${tsize}"
	    sleep 1
	    line1="${line1}|$(grep MB results.txt)"
	    line2="${line2}|$(grep msg results.txt)"
	    line3="${line3}|$(grep AVG results.txt)"
	    line4="${line4}|$(grep MIN results.txt)"
	    line5="${line5}|$(grep MAX results.txt)"
	    line6="${line6}|$(grep 95th results.txt)"
	    line7="${line7}|$(grep 99th results.txt)"
    done
  write_log "${line0}"
  write_log "${line1}"
  write_log "${line2}"
  write_log "${line3}"
  write_log "${line4}"
  write_log "${line5}"
  write_log "${line6}"
  write_log "${line7}"

  # Display results
  column -s '|' -t ${logfile}
  echo ""

  # Cleanup
  rm ${logfile}
  done
done

exit
