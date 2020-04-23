#!/bin/sh

set -eu

jot 50 > tmp.log

(sleep 1; printf "\033[5;2~"; sleep 1; ) \
	| ktrace -i ./ptty ./scroll ksh -c "tail -fn 50 tmp.log" > out.log
