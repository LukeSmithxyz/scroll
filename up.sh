#!/bin/sh

set -eu
export POSIXLY_CORRECT=1

jot 50 > tmp.log

(sleep 1; printf "\033[5;2~"; sleep 1; ) \
	| ./ptty ./scroll tail -fn 50 tmp.log > out.log
