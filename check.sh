#!/bin/bash

echo 'BUILDING'
make > /dev/null


echo 'RUNNING'
./mem input/proc/m0 > output/m0
./mem input/proc/m1 > output/m1
./os sched_0 > output/sched_0
./os sched_1 > output/sched_1
./os os_0 > output/os_0
./os os_1 > output/os_1


echo 'CHECKING ON MEM'
diff output/m0 sample/m0 > mem0.check
diff output/m0 sample/m0 > mem1.check


echo 'CHECKING ON SCHED'
diff output/sched_0 sample/sched_0 > shced0.check
diff output/sched_1 sample/sched_1 > sched1.check


echo 'CHECKING ON OS'
diff output/os_0 sample/os_0 > os1.check
diff output/os_1 sample/os_1 > os1.check
