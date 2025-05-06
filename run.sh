#! /bin/bash

# Tests file binaries and test sizes
#
#
#

date
starttime=`date`

ulimit -s unlimited
ipcrm --all

ipc_tests="shm uds fifo "
#ipc_tests="shm fifo uds"

udp_sizes="128 256 512 1024 2048 4096"
uds_sizes="64 128 256 512 1024 2048 4096 8192 32768"
socketpair_sizes="128 256 512 1024 2048 4096 8192 32768"
ipc_sizes="128 256 512 1024 2048 4096 8192 32768 65536"
ipc_sizes_full="128 256 512 1024 2048 4096 8192 32768 65536"

mins="1"

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

for iter in 1 2 3 4 5
do
# Call the test
  for test in ${ipc_tests}
  do
    # Initialize
    case $test in
	    "udp")
                ipc_sizes=${udp_sizes}
		;;
	    "uds")
                ipc_sizes=${uds_sizes}
		;;
            "socketpair")
                ipc_sizes=${socketpair_sizes}
		;;
	    "tcp")
                ipc_sizes=${ipc_sizes_full}
		;;
            "pipe")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    "fifo")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    "fifo-2way-1stamp")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    "shm")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    "shm-2way-1stamp")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    "posixq")
                ipc_sizes=${ipc_sizes_full}
		    ;;
	    *)
                ipc_sizes=${ipc_sizes_full}
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

    write_log "${test}" 

    for tsize in ${ipc_sizes}
    do
            ipcrm --all
	    ./${test} ${tsize} ${mins} ${iter} > results.txt
	    line0="${line0}|${tsize}"
	    sleep 1
	    line1="${line1}|$(grep MB results.txt)"
	    line2="${line2}|$(grep msg results.txt)"
	    line3="${line3}|$(grep AVG results.txt)"
	    line4="${line4}|$(grep MIN results.txt)"
	    line5="${line5}|$(grep MAX results.txt)"
	    line6="${line6}|$(grep 95th results.txt)"
	    line7="${line7}|$(grep 99th results.txt)"
            gzip ${test}_latencies_${tsize}_${iter}.json
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

date
endtime=`date`

echo "script started:  " $starttime
echo "script ended  :  " $endtime

cat /etc/bui* 

exit
