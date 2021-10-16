/*
vim:ts=3:sw=3:nowrap:syntax=c

# https://github.com/StarPet/semaphore
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

/* As /tmp is not anways in RAM I'll prefer /dev/shm */
/* #define DEF_SEMPREFIX			"/tmp/semaphore." */
#define DEF_SEMPREFIX		"/dev/shm/semaphore." 
#define DEF_LOCKNAME			"semaphore"
#define DEF_DEBUG_FILE 		"/var/logs/semaphore.log"

char *SEMPREFIX  = DEF_SEMPREFIX;
char *LOCKNAME   = DEF_LOCKNAME;
char *DEBUG_FILE = DEF_DEBUG_FILE;

long int minWait =   10000L; /* 10ms */
long int maxWait = 1000000L; /* 1s */

#define DEBUGPRT(level, ...) \
{ \
	if (debuglevel >= level) { \
		char now[80];  \
		struct timespec ts; \
		FILE *fp; \
		clock_gettime (CLOCK_REALTIME, &ts); \
		strftime (now, 80, "%Y-%m-%d %H:%M:%S", localtime (&ts.tv_sec)); \
		if ((fp = fopen (DEBUG_FILE, "a")) != NULL) { \
			fprintf (fp, "%s.%09ld %-20s r%4d p%5d pp%5d l%02d:",now,ts.tv_nsec,__FUNCTION__, __LINE__, getpid (), getppid(), level); \
			fprintf (fp, __VA_ARGS__); \
			fclose (fp); \
		} \
	} \
}

int debuglevel;

/* --------------------------------- */
void
usage()
{
	fprintf (stderr, "usage: semaphore -{s sec|c|h|r|t|T|p} [-k sig] name\n");
	fprintf (stderr, "  -s sec  : Set and test semaphore whithout waiting for it. Return code = 0 means not used, you own the lock now. If lock is older than sec seconds we clear it.\n");
	fprintf (stderr, "  -w sec  : Set and test semaphore and  waiting for it. If lock is older than sec seconds we clear it.\n");
	fprintf (stderr, "  -c      : Clear/release semaphore.\n");
	fprintf (stderr, "  -r      : Read semaphore.\n");
	fprintf (stderr, "  -k sig  : If sig >0: Kill parent of old lock holder if timed out. sig = signal (number) to use for kill(2). If sig = 0, no kill.\n");
	fprintf (stderr, "  -t      : Show timer of semaphore. t=time, c=counter, p=PID, pp=parent PID\n");
	fprintf (stderr, "  -T      : Show timer in seconds since epoch.\n");
	fprintf (stderr, "  -l 0-9  : Set debuglevel.\n");
	fprintf (stderr, "  -p      : Print parent pid (pp).\n");
	fprintf (stderr, "  -i usec : Minimum wait time for -w in microseconds (default: %ld).\n", minWait);
	fprintf (stderr, "  -a usec : Maximum wait time for -w in microseconds (default: %ld).\n", maxWait);
	fprintf (stderr, "  -h      : This short help.\n");
	fprintf (stderr, "Source can be found at: https://github.com/StarPet/semaphore\n");
	fprintf (stderr, "Note: Prefix used.........: \"%s\"\n", SEMPREFIX);
	fprintf (stderr, "Note: Value to sem_open(2): \"%s\"\n", LOCKNAME);
	fprintf (stderr, "Note: Only PIDs >1 can be killed.\n\n");
	fprintf (stderr, "Note: Debug log at %s\n", DEBUG_FILE);
}

/* --------------------------------- */
static void
handler(int sig)
{
	DEBUGPRT (9, "In handler...\n");
}

/* --------------------------------- */
int
semCore (const char *semname, const int mode,  int dokill, int timo) {
	sem_t *sem;
	char *sfname;
	int ret, dowrite;
	int semfile, wr, nr;
	int mysem_size;
	int semstat;
	struct stat statvar;
	extern char *optarg;
	extern int optind, opterr, optopt;
	struct timespec tspec;
	struct sigaction sa;
	long open_opts;
	time_t now, uptime;
	char timestring[80];
	double tdif;
	pid_t him_ppid;
	struct {
		long   ms_counter;
		time_t ms_starttime;
		pid_t  ms_pid, ms_ppid;
	} mysem;

#ifndef nosemopen
	/* Unfortunatly this is not implemented on the SLUG = armv5tel, see makefile */

	/* First we use a constant (global) name to assure our own consistency */
	if ((sem = sem_open (LOCKNAME, O_CREAT, 0600, 0)) == SEM_FAILED) {
		perror ("");
		fprintf (stderr, "Error: sem_open failed using \"%s\".\n", LOCKNAME);
		return -1;
	}

	DEBUGPRT (4, "sem = %ld\n", (long int) sem);
	if ((ret = sem_trywait(sem)) != 0) {
		DEBUGPRT (5, "sem_trywait = %d\n", ret);
		sa.sa_handler = handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		if (sigaction(SIGALRM, &sa, NULL) == -1) {
			return -1;
		}
		/* even in a slow system we should be done with our stuff in less than a second */
		alarm(1);
		tspec.tv_sec  = time(NULL) + 2;
		DEBUGPRT (5, "tv_sec=%ld\n", (long) tspec.tv_sec);
		tspec.tv_nsec = 0L;
		while ((ret = sem_timedwait(sem, &tspec)) == -1 && errno == EINTR) {
			continue;
		}
		DEBUGPRT (5, "errno=%d\n", errno);
	}
	DEBUGPRT(5, "inner core (ret=%d).\n", ret);
#endif

	sfname = (char *) malloc (strlen (SEMPREFIX) + strlen (semname) + 1);
	if (sfname == NULL) {
		fprintf (stderr, "Error: unable to allocate memory.");
		return -1;
	}
	sprintf (sfname, "%s%s", SEMPREFIX, semname);

	semstat = stat(sfname, &statvar);
	DEBUGPRT (4, "semstat = %d\n", semstat);

	if (semstat == 0) {
		open_opts = O_EXCL | O_SYNC | O_RDWR;
	} else {
		/* open_opts = O_EXCL | O_SYNC | O_WRONLY | O_CREAT | O_TRUNC; */
		open_opts = O_EXCL | O_SYNC | O_WRONLY | O_CREAT;
	}
	if ((semfile = open(sfname, open_opts, 0666)) < 0) {
		perror (sfname);
		DEBUGPRT (1, "errno = %d", errno);
		return -1;
	}
	(void) time(&now);
	mysem_size = sizeof(mysem);
	memset (&mysem, '\0', mysem_size); /* default: empty */
	if (semstat == 0) {
		nr = read (semfile, &mysem, mysem_size);
		if (nr != mysem_size && nr != 0) {
			DEBUGPRT (1, "read only nr=%d bytes (instead of %d) semaphore '%s'\n", nr, mysem_size, semname);
			memset (&mysem, '\0', mysem_size); /* as it was invalid */
		}
		DEBUGPRT (4, "read from file\n");
	} else {
		mysem.ms_counter = 0;
		mysem.ms_starttime = now;
		mysem.ms_pid  = 0;
		mysem.ms_ppid = 0;
	}
	ret = -1;
	DEBUGPRT (5, "counter=%ld start time=%ld pid=%d ppid=%d\n", mysem.ms_counter, mysem.ms_starttime, mysem.ms_pid, mysem.ms_ppid);

	dowrite = 0;
	switch (mode) {
		case 's' :
			tdif = difftime (now, mysem.ms_starttime);
			if (tdif < 0) {
				DEBUGPRT (2, "Invalid time %f semaphore '%s'\n", tdif, semname);
				mysem.ms_counter = 0;
			} 
			if (mysem.ms_counter < 0) {
				DEBUGPRT (2, "Invalid counter %ld semaphore '%s'\n", mysem.ms_counter, semname);
				mysem.ms_counter = 0;
			}
			if (timo > 0 && mysem.ms_counter != 0) {
				uptime = mysem.ms_starttime + timo;
				tdif = difftime (now, uptime);
				if (tdif > 0) { // Note: timo already includes the time extension
					DEBUGPRT (2, "time for semaphore '%s' expired by %1.2f over %d\n", semname, tdif, timo);
					mysem.ms_counter = 0;
					him_ppid = mysem.ms_ppid;

					if (dokill > 0) {
						DEBUGPRT (1, "killing holder (%d) of semaphore '%s'\n", him_ppid, semname);
						// kill holder
						if (him_ppid > 1) {
							kill (him_ppid, dokill);
						}
					} // 0 = no kill
				}
			}
			if (mysem.ms_counter == 0) {
				mysem.ms_counter++;
				mysem.ms_starttime = now;
				mysem.ms_pid  = getpid();
				mysem.ms_ppid = getppid();
				ret = 0;	/* ok, it's us. lock set, we go */
				dowrite = 1;
			} else {
				ret = 1;	/* no, wait (outside) for lock to free up. */
			}
			DEBUGPRT (4, "set counter = %ld, ret=%d for '%s'\n", mysem.ms_counter, ret, semname);
			break;
		case 'c' :
			ret = 0;
			if (mysem.ms_counter > 0) {
				mysem.ms_counter--;
				mysem.ms_starttime = now;
				mysem.ms_pid  = 0;
				mysem.ms_ppid = 0;
			}
			DEBUGPRT (4, "clear counter = %ld for '%s'\n", mysem.ms_counter, semname);
			dowrite = 1;
			break;
		case 'r':
			if (mysem.ms_counter == 0) {
				ret = 0;	/* free */
			} else {
				ret = 1;	/* blocked */
			}
			break;
		case 't':
			strftime (timestring, 80, "%Y-%m-%d %H:%M:%S", localtime (&(mysem.ms_starttime)));
			printf ("t:%s c:%ld p:%d pp:%d\n", timestring, mysem.ms_counter, mysem.ms_pid, mysem.ms_ppid);
			ret = 0;	/* ok */
			break;
		case 'T':
			strftime (timestring, 80, "%s", localtime (&(mysem.ms_starttime)));
			printf ("%s\n", timestring);
			ret = 0;	/* ok */
			break;
		case 'p':
			printf ("%d\n", mysem.ms_ppid);
			ret = 0;	/* ok */
			break;
		default :
			fprintf (stderr, "Error: Assertion failed. Aborting.\n");
			return -1;
			break;
	}
	if (dowrite == 1) {
		(void) lseek (semfile, 0L, SEEK_SET);
		if ((wr = write (semfile, &mysem, mysem_size)) != mysem_size) {
			DEBUGPRT (1, "wr = %d\n", wr);
		}
		DEBUGPRT (7, "ret=%d errno=%d\n", ret, errno);
	}

	close (semfile);
#ifndef nosemopen
	sem_post (sem);
	sem_close (sem);
#endif
	/* let it remain until reboot
		sem_unlink (LOCKNAME);
	*/
	return ret;
}

/* --------------------------------- */
int
main (int argc, char **argv, char **env)
{
	char *semname;
	int opt, mode=0, timo, ret;
	int dokill;
	extern char *optarg;
	extern int optind, opterr, optopt;
	char *envvar;
	long int waitDur;

	debuglevel = 3;
	dokill=0;
	if ((envvar = getenv("DEBUGLEVEL")) != NULL) { debuglevel = atoi (envvar); }
	if ((envvar = getenv("SEMPREFIX"))  != NULL) { SEMPREFIX  = envvar; }
	if ((envvar = getenv("LOCKNAME"))   != NULL) { LOCKNAME   = envvar; }
	if ((envvar = getenv("DEBUG_FILE")) != NULL) { DEBUG_FILE = envvar; }

	while ((opt = getopt(argc, argv, "s:w:chrtk:l:Tpi:a:")) != -1) {
		DEBUGPRT (8, "opt=%d %c\n", opt, opt);
		switch (opt) {
			case 's' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				timo = atol(optarg);
				break;
			case 'w' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				timo = atol(optarg);
				break;
			case 'c' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				break;
			case 'r' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				break;
			case 't' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				break;
			case 'T' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				break;
			case 'p' :
				if (mode == 0) {
					mode = opt;
				} else {
					usage();
					return -1;
				}
				break;
			case 'l' :
				debuglevel = atoi(optarg);
				break;
			case 'k' :
				dokill = atoi(optarg);
				break;
			case 'i' :
				minWait = atol(optarg);
				break;
			case 'a' :
				maxWait = atol(optarg);
				break;
			case 'h' :
				usage();
				return 0;
				break;
			default:
				usage();
				return -1;
				break;
		}
	}
	if (argc != (optind + 1)) {
		usage();
		return -1;
	}
	if (minWait >= maxWait) {
		fprintf (stderr, "Error: argument to -i (%ld) must be smaller than argument to -a (%ld)", minWait, maxWait);
		return -1;
	}

	DEBUGPRT (8, "optind=%d argc=%d\n", optind, argc);
	if (strlen (argv[optind]) > 256) {
		fprintf (stderr, "Error: Name too long.");
		return -1;
	}

	semname = (char *) malloc (256);
	if (semname == NULL) {
		fprintf (stderr, "Error: unable to allocate memory.");
		return -1;
	}
	strcpy (semname, argv[optind]);
	if (strchr (semname, '/') != 0) {
		fprintf (stderr, "Error: name must not contain \"/\" characters (%s).\n", semname);
		return -1;
	}
	DEBUGPRT (4, "semname = '%s'\n", semname);
	switch (mode) {
		case 'w':
			while ((ret = semCore (semname, 's', dokill, timo)) != 0) {
				waitDur = (int) (minWait + (maxWait * random() / RAND_MAX));
				DEBUGPRT (9, "waiting %ldµs\n", waitDur);
				usleep (waitDur);
			}
			break;
		default:
			ret = semCore (semname, mode, dokill, timo);
			break;
	}
	free (semname);

	return ret;
}

/* End */
