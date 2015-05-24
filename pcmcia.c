
#include "pcmcia.h"

#ifdef DEBUG
#undef DEBUG
#define DEBUG(s...) fprintf(stderr, s)
#else
#define DEBUG(s...) ;
#endif

/*====================================================================*/

static int major = 0;

static int lookup_dev(char *name)
{
    FILE *f;
    int n;
    char s[32], t[32];
    
    f = fopen("/proc/devices", "r");
    if (f == NULL)
	return -errno;
    while (fgets(s, 32, f) != NULL) {
	if (sscanf(s, "%d %s", &n, t) == 2)
	    if (strcmp(name, t) == 0)
		break;
    }
    fclose(f);
    if (strcmp(name, t) == 0)
	return n;
    else
	return -ENODEV;
} /* lookup_dev */

/*====================================================================*/

int open_sock(int sock)
{
    static char *paths[] = {
	"/var/lib/pcmcia", "/var/run", "/dev", "/tmp", NULL
    };
    int fd;
    char **p, fn[64];
    dev_t dev = (major<<8) + sock;

    for (p = paths; *p; p++) {
	sprintf(fn, "%s/cc-%d", *p, getpid());
	if (mknod(fn, (S_IFCHR|S_IREAD|S_IWRITE), dev) == 0) {
	    fd = open(fn, O_RDONLY);
	    unlink(fn);
	    if (fd >= 0)
		return fd;
	    if (errno == ENODEV)
		break;
	}
    }
    return -1;
} /* open_sock */

/*====================================================================*/


int get_tuple(int fd, cisdata_t code, ds_ioctl_arg_t *arg)
{
    arg->tuple.DesiredTuple = code;
    arg->tuple.Attributes = TUPLE_RETURN_COMMON;
    arg->tuple.TupleOffset = 0;
    if ((ioctl(fd, DS_GET_FIRST_TUPLE, arg) == 0) &&
	(ioctl(fd, DS_GET_TUPLE_DATA, arg) == 0) &&
	(ioctl(fd, DS_PARSE_TUPLE, arg) == 0))
	return 0;
    else
	return -1;
}


int
init_pcmcia(void)
{
    major = lookup_dev("pcmcia");
    if (major < 0) {
		if (major == -ENODEV)
		{
			DEBUG("no pcmcia driver in /proc/devices\n");
		}
		else
		{
			DEBUG("could not open /proc/devices\n");
		}
		return 0;
    }
	return major;
}
