#!/bin/bash

#########################
func_nowait () {
	local num=$1

	if semaphore -s 1 $LOCK; then
		echo "$num won the lottery! ;-)"
		sleep 2
		echo "$num has finished sleeping. Clearing lock."
		semaphore -c $LOCK
	else
		echo "$num lost."
	fi
}

#########################
func_wait () {
	local num=$1

	while ! semaphore -s 10 "$LOCK"; do
		sleep .02$$ # Use the processes PID to get a randomized sleep time
	done
	echo "I'm $num and this is my turn."
	semaphore -c $LOCK
}

#########################
#########################
#########################
# main

LOCK="testsem"

# Clean up on uncontrolled exiting
trap "semaphore -c $LOCK" EXIT

echo "No Wait"
# 10 fighting for the lock without waiting
for n in $(seq 1 10); do
	func_nowait $n &
done

# This wait just waits for all background jobs to complete. See bash man page.
wait

echo
echo "With Wait"
# 10 waiting for the lock
for n in $(seq 1 10); do
	func_wait $n &
done
wait

echo "Simple testing example"
semaphore -s 10 $LOCK
rc=$?
echo "Set lock: rc=$rc"

semaphore -r $LOCK
rc=$?
echo "Read/test lock: rc=$rc"
# Clear it
semaphore -c $LOCK

echo "Done."

# End
