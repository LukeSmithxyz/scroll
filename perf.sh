#!/bin/sh

set -eu

num=1000000
seq=seq

if [ -x /usr/bin/jot ]; then
	seq=jot
fi

rm -f perf_*.log

for i in `$seq 10`; do
	/usr/bin/time st -e                      jot $num 2>>perf_0.log
done

for i in `$seq 10`; do
	/usr/bin/time st -e ./ptty               jot $num 2>>perf_1.log
done

for i in `$seq 10`; do
	/usr/bin/time st -e ./ptty ./ptty        jot $num 2>>perf_2.log
done

for i in `$seq 10`; do
	/usr/bin/time st -e ./ptty ./ptty ./ptty jot $num 2>>perf_3.log
done
