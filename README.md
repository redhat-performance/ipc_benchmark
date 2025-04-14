# ipc_benchmark
IPC benchmark on Linux which is inspired by [APUE](http://www.apuebook.com/)

# Test

Just Make style as the following :D

```bash
make && make test
```

# Type

These are kinds of IPC in Linux as the following:

type|feature
| ------------- |:-------------:
pipe|unnamed pipe
fifo|named pipe
socketpair | unnamed unix domain socket
unix domain socket | named unix domain socket
TCP | remote domain socket
UDP | loopback interface

# Benchmark

In my personal MacBook Pro (Retina, 13-inch, Late 2013) Intel(R) Core(TM) i5-4258U CPU @ 2.40GHz, the benchmark is as the following:

```c
pipe
128             512           1024          4096
1319Mb/s        5110Mb/s      8932Mb/s      20297Mb/s
1288233msg/s    1247449msg/s  1090370msg/s  619407msg/s

fifo
128             512           1024          4096
1358Mb/s        5491Mb/s      8440Mb/s      22018Mb/s
1326016msg/s    1340502msg/s  1030215msg/s  671929msg/s

socketpair
128             512           1024          4096
1117Mb/s        4401Mb/s      8869Mb/s      17207Mb/s
1090370msg/s    1074548msg/s  1082674msg/s  525121msg/s

uds
128             512           1024          4096
100Mb/s         102Mb/s       109Mb/s       97Mb/s
97600msg/s      24986msg/s    13289msg/s    2968msg/s

tcp
128             512           1024          4096
106Mb/s         143Mb/s       135Mb/s       128Mb/s
103508msg/s     34852msg/s    16529msg/s    3897msg/s
```

Also we make the benchmark on travis-ci [![Build Status](https://travis-ci.org/detailyang/ipc_benchmark.svg?branch=master)](https://travis-ci.org/detailyang/ipc_benchmark)


# Contributing
------------

To contribute to ipc_benchmark, clone this repo locally and commit your code on a separate branch. 


# License
-------

ipc_benchmark is based on [ipc_benchmark](https://github.com/detailyang/ipc_benchmark)
licensed under the [MIT](LICENSE.MIT) license, Copyright (c) 2017
[@detailyang](https://github.com/detailyang)

The [original fork of this
repository](https://github.com/redhat-performance/ipc_benchmark/tree/4e1f6788c5b792959ac19fc5ba6d9f958ce4ba83)
represents the original MIT licensed state of the code.

New work in this repository is licensed under the [Apache 2.0](LICENSE.Apache-2.0)
license.

Please [review the diff of
changes](https://github.com/redhat-performance/ipc_benchmark/compare/4e1f678..main) to
understand the license application.

`SPDX-License-Identifier: Apache-2.0 AND MIT`
