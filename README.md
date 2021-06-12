Overview
========

`semaphore` can be used in shell scripts for locking

About
=====

There are situations in bash scripts where you have to make sure
that that section is executed only once even though multiple processes
may be running in parallel executing the same script.

In order to assure integrity I have being using the following common method:
 if mkdir /tmp/whatever; then
		doStuff that must be run only once at a given time
		rmdir /tmp/wahtever
 fi

This however does not always work. I noticed several times that multiple
processes entered the then-fi block at the same time.

Build
=====

Just run
 make
and it should compile it (provided gcc and install tool are present).
Output looks like
make
 gcc  -o semaphore -lpthread -lrt semaphore.c
 install -m 755 semaphore /usr/bin

Usage
=====

 usage: semaphore -{s sec|c|h|r|t|T|p} [-k sig] name
   -s sec  : Set and test semaphore whithout waiting for it. Return code = 0 means not used, you own the lock now. If lock is older than sec seconds we clear it.
   -c      : Clear/release semaphore.
   -r      : Read semaphore.
   -k sig  : If sig >0: Kill parent of old lock holder if timed out. sig = signal (number) to use for kill(2). If sig = 0, no kill.
   -t      : Show timer of semaphore. t=time, c=counter, p=PID, pp=parent PID
   -T      : Show timer in seconds since epoch.
   -l 0-9  : Set debuglevel.
   -p      : Print parent pid (pp).
   -h      : This short help.
 Source can be found at: https://github.com/StarPet/semaphore
 Note: Prefix used.........: "/dev/shm/semaphore."
 Note: Value to sem_open(2): "semaphore"
 Note: Only PIDs >1 will be killed.
 Note: Debug log at /var/logs/semaphore.log
 homesrv:/home/fhz/bin # 

Use Cases
=========

You may want to check out the `example.sh` bash script for a some example uses.

In the simple examples below I use `$LOCK` for the name of the LOCK. 
Meaning prior you need to have a `LOCK=somename` prior to get to that reference.
If you have multiple places where you use `semaphore` for a different purpose, make sure to
use a different value for `$LOCK` for each prurpose.

Here are some use cases:
	# Test and set: Here `-s 1` means a lock older than 1 sec is treated as free.
	semaphore -s 1 $LOCK
	rc=$?
	# Here you should check the return code (stored in rc) if the lock is set.
	# rc == 0: the lock is ours and set.
	# rc != 0: the lock is set by some other process
	<your code>
	# Once done, clear the lock. But only if you own it (rc was 0)
	semaphore -c $LOCK

	# Test but do not set the LOCK
	semaphore -t $LOCK
	rc=$?
	# Use

# Important note:

The `semaphore` program does NOT check if the user is permitted to perform the change in the lock or not.
Just the access to the used lock file (see SEMPREFIX below) is needed.

Environment
===========
DEBUGLEVEL Can be used as an alternative to `-l n` (see usage).

DEBUG_FILE File used for debug log information, if DEBUGLEVEL > 0.

SEMPREFIX Prefix for the lock file. Defaults to `/dev/shm/semaphore.` which,
together with the `name` of the lock (see usage) attached to the end it, will create a lock file for each `name`.

LOCKNAME Global lock used for the sem_open (3) call

