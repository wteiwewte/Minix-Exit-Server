#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1    /* get OK and negative error codes */

#include <minix/sysutil.h>
#include <minix/syslib.h>

#include <minix/obs.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>

