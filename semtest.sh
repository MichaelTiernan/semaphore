#!/bin/bash
# vim:ts=3:sw=3:nowrap:syntax=sh

SEMPRG="./semaphore -l 9"
SEM=mysem
tfile=/dev/shm/tfile.$$
export PS4='+$(date +%Y.%m.%d-%H:%M:%S.%N):P$$:${FUNCNAME}:${LINENO}> '
export DEBUG_FILE="semtest.log"
rm -f semtest.log

trap "rm -f $tfile" EXIT

test_func() {
	echo "start $$ $tfile"
	while ! $SEMPRG -s 4 $SEM; do
		echo "waiting."
		sleep .01
	done
	[ -f $tfile ] && echo "Error".
	touch $tfile
	sleep 1
	rm -f $tfile
	$SEMPRG -c $SEM
	echo "end $$"
}
#####
bg_func() {
	$SEMPRG -s 10 $SEM || echo "Error"
	sleep 4
	echo "Error: We should not get here."
	$SEMPRG -c $SEM || echo "Error"
}

#####
set -x
$SEMPRG -c $SEM    || echo "Error"
$SEMPRG -s 4 $SEM  || echo "Error"
sleep 2
$SEMPRG -s 4 $SEM  && echo "Error"
sleep 3
$SEMPRG -s 4 $SEM  || echo "Error"
$SEMPRG -c $SEM    || echo "Error"

set +x
for job in $(seq 1 3); do
	test_func &
done
wait

echo "Kill test"
set -x
bg_func &
sleep 3
$SEMPRG -k 9 -s 2 $SEM  || echo "Error"
$SEMPRG -c $SEM    || echo "Error"
wait

echo "Built-in wait test"
$SEMPRG -s 5 $SEM || echo "Error" # Set the sem to make the -w later wait for its release
{ sleep 2; $SEMPRG -c $SEM; } &    # Clear the sem in 2s so wait can get it
echo "pre-wait (should take approx 2s) and have small 'user' and 'sys' times"
time $SEMPRG -w 10 $SEM
echo "post-wait"
$SEMPRG -c $SEM

# End
