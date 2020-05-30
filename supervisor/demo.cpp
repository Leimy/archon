#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <sys/procdesc.h>
#include <sys/procctl.h>
#include <cstdio>
#include <errno.h>
#include <signal.h>

using namespace std;

siginfo_t sig_info;
volatile sig_atomic_t sig_num;
void *sig_ctxt;

static void catcher(int signum, siginfo_t *info, void *vp)
{
    sig_num = signum;
    sig_info = *info;
    sig_ctxt = vp;
}

void setup_signals()
{
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = catcher;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGCHLD, &sa, 0) != 0) {
		errx(EX_OSERR, "sigaction");
	}
}

int main ()
{
	int presharedfds[2];
	int other_pid_fd = -1;
	struct iovec io;
	struct msghdr msg;
	union {
		struct cmsghdr hdr;
		unsigned char buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;
	struct cmsghdr *cmsg;

	// Unix domain socketpair
	if (socketpair(PF_LOCAL, SOCK_STREAM, 0, presharedfds) == -1)
		err(EX_OSERR, "failed to create a pre-shared socket pair");
	char buf[1];
	io.iov_base = buf;
	io.iov_len = 1;
	
	memset(&msg, 0, sizeof(msg));
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;

	setup_signals();

	switch(fork()) {
	case -1:
		errx(EX_OSERR, "fork");
	case 0: // Child/Receiver
	{
		int rv = -1;
		close(presharedfds[0]);
		// receive sibling process FD.
		if ((rv = recvmsg(presharedfds[1], &msg, 0)) == -1)
			err(EX_IOERR, "failed to receive a message");
		if (msg.msg_flags & (MSG_CTRUNC | MSG_TRUNC))
			errx(EX_IOERR, "control message truncated");
		printf("Firstborn: Received a message : %d!\n", rv);
		cmsg = CMSG_FIRSTHDR(&msg);
		if (cmsg == NULL)
			errx(EX_IOERR, "Received no messages!!!");
		printf("Firstborn: Len: %u, type == SCM_RIGHTS: %s\n", cmsg->cmsg_len, cmsg->cmsg_type == SCM_RIGHTS ? "true" : "false");
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
		    cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			pid_t other_pid;
			other_pid_fd = *(int *)CMSG_DATA(cmsg);
			if (pdgetpid(other_pid_fd, &other_pid) == -1)
				errx(EX_OSERR, "Firstborn: pdgetpid");
			printf("Firstborn: Got sibling fd: %d pid: %d\n", other_pid_fd, other_pid);
			printf("Firstborn: Attempt to become reaper\n");
			if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL) == -1)
				warn("Firstborn: Failed to become reaper: %d\n", errno);
			struct procctl_reaper_status prs;
			if (procctl(P_PID, getpid(), PROC_REAP_STATUS, &prs) == -1)
				errx(EX_OSERR, "procctl: %d", errno);
			printf("Firstborn: rs_flags: %u rs_children: %u rs_descendants: %u rs_reaper: %d rs_pid: %d\n",
			       prs.rs_flags, prs.rs_children, prs.rs_descendants, prs.rs_reaper, prs.rs_pid);
			sleep(5);
			printf("Firstborn: nice knowing you for 3 seconds... killing\n");
			pdkill(other_pid_fd, 15); // TERM it
		}
		sleep(2);
		printf("Firstborn: my work is done\n");
		return 0;
	}
		break;
	default: // Parent
	{
		close(presharedfds[1]);
		int otherfd = -1;
		int status = -1;
		pid_t pid = pdfork(&otherfd, 0);
		if (pid == -1)
			errx(EX_OSERR, "pdfork");
		if (pid == 0) {
			struct procctl_reaper_status prs;
			if (procctl(P_PID, getpid(), PROC_REAP_STATUS, &prs) == -1)
				errx(EX_OSERR, "procctl: %d", errno);
			printf("I'm the sibling, the reaper is %d\n", prs.rs_reaper);
			sleep(10000);  // lazy kid
			return 5;
		}
		printf("Other child pid is %d fd: %d\n", pid, otherfd);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;

		printf("About to send fd to the other kid\n");
		// send the child information about its sibling
		*(int *)CMSG_DATA(cmsg) = otherfd;
		if (sendmsg(presharedfds[0], &msg, 0) == -1)
			err(EX_IOERR, "sendmsg");
		
		printf("sent firstborn pdesc of sibling\n");
		// Counting on our firstborn to murder the little one

		sleep(1);
		close(otherfd); // I think if I hold on to this it won't die.
	retry:
		pid = waitpid(pid, &status, 0);
		if (pid == -1) {
			if (errno == EINTR)
				goto retry;
			printf("waitpid errno: %d\n", errno);
			errx(EX_OSERR, "waitpid");
		}

		printf("waited successfully: %d\n", status);
			
		if (WIFEXITED(status)) {
			printf("WEXITSTATUS: %d\n", WEXITSTATUS(status));
			errx(EX_OSERR, "Shouldn't have exited");
		}

		if (WTERMSIG(status) == 9)
			printf("First born killed second! Success!\n");
		else 
			printf("The secondborn died with %d\n", WTERMSIG(status));

	}
		break;
	}
}

    
