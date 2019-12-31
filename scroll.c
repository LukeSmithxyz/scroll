//#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// TODO: OpenBSD/Linux ifdef
#include <util.h>
//#include <pty.h>

typedef struct Line Line;
struct Line {
	Line *next;
	Line *prev;
	size_t len;
	char str[];
};

pid_t child;
int mfd;
struct termios dfl;
struct winsize ws;
Line *lines, *bottom;

void
die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
	exit(1);
}

void
sigchld(int sig)
{
	assert(sig == SIGCHLD);

	pid_t pid;
	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
		if (pid == child)
			die("child died");
}

void
sigwinch(int sig)
{
	assert(sig == SIGWINCH);

	if (ioctl(1, TIOCGWINSZ, &ws) < 0)
		die("ioctl:");
	if (ioctl(mfd, TIOCSWINSZ, &ws) < 0)
		die("ioctl:");
	kill(-child, SIGWINCH);
}

void
reset(void)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &dfl) < 0)
		die("tcsetattr:");
}

void
addline(char *str)
{
	size_t len = strchr(str, '\n') - str + 1;
	Line *lp = malloc(sizeof(*lp) + len * sizeof(*lp->str));

	if (!lp)
		die("malloc:");
	memcpy(lp->str, str, len);
	lp->len = len;
	if (lines)
		lines->next = lp;
	lp->prev = lines;
	lp->next = NULL;
	bottom = lines = lp;
}

void
scrollup(void)
{
	Line *lp;
	int rows = ws.ws_row-1;
	int cols = ws.ws_col;

	if (!bottom || !(bottom = bottom->prev))
		return;

	for (lp = bottom; lp && rows > 0; lp = lp->prev)
		rows -= lp->len / cols + 1;

	if (rows < 0) {
		write(STDOUT_FILENO, lp->str + -rows * cols, lp->len - -rows * cols);
		rows = 0;
		lp = lp->next;
	}

	for (; lp && lp != bottom->next; lp = lp->next)
		write(STDOUT_FILENO, lp->str, lp->len);
}

int
main(int argc, char *argv[])
{
	if (argc <= 1)
		die("usage: scroll <program>");

	if (tcgetattr(STDIN_FILENO, &dfl) < 0)
		die("tcgetattr:");
	if (atexit(reset))
		die("atexit:");

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
		die("ioctl:");

	child = forkpty(&mfd, NULL, &dfl, &ws);
	if (child == -1)
		die("forkpty:");
	if (child == 0) {	/* child */
		execvp(argv[1], argv + 1);
		perror("execvp");
		_exit(127);
	}

	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("signal:");
	if (signal(SIGWINCH, sigwinch) == SIG_ERR)
		die("signal:");

	int f;
	if ((f = fcntl(mfd, F_GETFL)) < 0)
		die("fcntl:");
	if (fcntl(mfd, F_SETFL, f /*| O_NONBLOCK*/) < 0)
		die("fcntl:");

	struct termios new;
	new = dfl;
	cfmakeraw(&new);
	new.c_cc[VMIN ] = 1;
	new.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &new) < 0)
		die("tcsetattr:");

	fd_set rd;
	char buf[10000], *p = buf;
	for (;;) {
		char c;

		FD_ZERO(&rd);
		FD_SET(STDIN_FILENO, &rd);
		FD_SET(mfd, &rd);

		if (select(mfd + 1, &rd, NULL, NULL, NULL) < 0 && errno != EINTR)
			die("select:");

		if (FD_ISSET(STDIN_FILENO, &rd)) {
			if (read(STDIN_FILENO, &c, 1) <= 0 && errno != EINTR)
				die("read:");
			if (c == 17) /* ^Q */
				scrollup();
			else if (write(mfd, &c, 1) < 0)
				die("write:");
		}
		if (FD_ISSET(mfd, &rd)) {
			if (read(mfd, &c, 1) <= 0 && errno != EINTR)
				die("read:");
			*p++ = c;
			if (c == '\n') {
				p = buf;
				addline(buf);
			}
			if (write(STDOUT_FILENO, &c, 1) < 0)
				die("write:");
		}
	}
	return 0;
}
