// -*- mode: c++; eval: (c-set-style "stroustrup"); eval: (c-set-style "archon-cc-mode"); -*-
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <kenv.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <signal.h>
#include <paths.h> // _PATH_BSHELL

#include "include/util.h"


using namespace std;

//
// Receive a file descriptor over a Unix Domain Socket
//
int
recvfd(int sock)
{
    int fd;
    int n;
    char buf[1];
    struct iovec io {.iov_base = buf, .iov_len = 1};
    struct msghdr hdr = {};
    struct cmsghdr *chdr;
    char cms[CMSG_SPACE(sizeof(int))];

    hdr.msg_iov = &io;
    hdr.msg_iovlen = 1;
    hdr.msg_control = (caddr_t)chdr;
    hdr.msg_controllen = sizeof(chdr);

    if ((n = recvmsg(sock, &hdr, 0)) < 0) {
	return -1;
    }

    if (n == 0) {
	// error, EOF?
	return -1;
    }

    chdr = CMSG_FIRSTHDR(&hdr);
    memmove(&fd, CMSG_DATA(chdr), sizeof(int));

    return fd;
}

//
// Runcom compatibility, mostly lifted from init
//
#define STALL_TIMEOUT 30 // wait N seconds after warning

// Logs a message then sleeps.
static void
stall(const char* message, ...) {
    va_list ap;
    va_start(ap, message);

    vsyslog(LOG_ALERT, message, ap);
    va_end(ap);
    sleep(STALL_TIMEOUT);
}

// Logs a message (no sleep)
static void
warning(const char* message, ...) {
    va_list ap;
    va_start(ap, message);

    vsyslog(LOG_ALERT, message, ap);
    va_end(ap);
}

// Logs an emergency message
static void
emergency(const char* message, ...) {
    va_list ap;
    va_start(ap, message);

    vsyslog(LOG_EMERG, message, ap);
    va_end(ap);
}

static enum { AUTOBOOT, FASTBOOT } runcom_mode = AUTOBOOT;

const char *
get_boot_shell(void) {
    static char kenv_value[PATH_MAX];

    if (kenv(KENV_GET, "init_shell", kenv_value, sizeof(kenv_value)) > 0)
	return kenv_value;
    else
	return _PATH_BSHELL;
}

static void
execute_script(char* argv[]) {
    struct sigaction sa;
    const char* script;
    const char* shell = get_boot_shell();
    int error;

    // Likely unneeded, as init already did this to us.
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    // init opens the console here, so we should not have to.

    sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

    script = argv[1];
    error = access(script, X_OK);
    if (error == 0) {
	execv(script, argv + 1);
	warning("can't directly exec %s: %m", script);
    } else if (errno != EACCES) {
	warning("can't access %s: %m", script);
    }
    execv(shell, argv);
    stall("can't exec %s for %s: %m", shell, script);
    _exit(1); // Single user mode
}

void
run_rc(const char* script) {
    pid_t pid;
    const char* shell = get_boot_shell();

    int status;
    char* argv[4];

    openlog("init", LOG_CONS, LOG_AUTH);

    if((pid = fork()) == 0) {
	char _sh[] = "sh";
	char _autoboot[] = "autoboot";

        argv[0] = _sh;
	argv[1] = __DECONST(char *, script);
	argv[2] = runcom_mode == AUTOBOOT ? _autoboot : 0;
	argv[3] = NULL;

	execute_script(argv);
	sleep(STALL_TIMEOUT);
	_exit(1);  // relay back to parent. parent should exit too (single user mode).
    }

    if (pid == -1) {
	emergency("can't fork for %s on %s: %m", shell, script);
	sleep(STALL_TIMEOUT);
	_exit(1); // Tells init to go single user.
    }

  retry:
    if (waitpid(pid, &status, WUNTRACED) == -1) {
	if (errno == EINTR)
	    goto retry;

	_exit(1);
    }

    if (WEXITSTATUS(status) || (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM)) {
	// Failed to run runcom correctly, should signal parent to go single user.
	_exit(1);
    }

    closelog();
}

int
main (int argc, char **argv) {
    int c;
    // might delete later, just a placeholder
    while ((c = getopt(argc, argv, "")) != -1) {
	switch (c) {
	default:
	    warning("unrecognized flag: '-%c'", c);
	    break;
	}
    }

    runcom_mode = FASTBOOT;
    for(; optind < argc; ++optind) {
	if (strncmp(argv[optind], "autoboot", sizeof("autoboot")) == 0) {
	    runcom_mode = AUTOBOOT;
	}
    }

    // If we survive this we're good.
    run_rc("/etc/rc");
    _exit(0); // go multi-user.

// initialize logging
// runcom for boot.
// daemonize, if successful.
// Create command unix domain socket.
}
