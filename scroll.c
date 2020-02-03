#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/queue.h>

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

#if   defined(__linux)
 #include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 #include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
 #include <libutil.h>
#endif

TAILQ_HEAD(tailhead, line) head;

struct line {
	TAILQ_ENTRY(line) entries;
	size_t len;
	char *str;
} *bottom;

pid_t child;
int mfd;
struct termios dfl;
struct winsize ws;

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

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
		die("ioctl:");
	if (ioctl(mfd, TIOCSWINSZ, &ws) == -1)
		die("ioctl:");
	kill(-child, SIGWINCH);
}

void
reset(void)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &dfl) == -1)
		die("tcsetattr:");
}

void
addline(char *str, size_t size)
{
	struct line *line = malloc(sizeof *line);

	if (line == NULL)
		die("malloc:");

	line->len = size;
	line->str = malloc(size);
	if (line->str == NULL)
		die("malloc:");
	memcpy(line->str, str, size);

	bottom = line;

	TAILQ_INSERT_HEAD(&head, line, entries);
}

void
scrollup(void)
{
	struct line *l;
	int rows = ws.ws_row-1;
	int cols = ws.ws_col;

	if (bottom == NULL || (bottom = TAILQ_NEXT(bottom, entries)) == NULL)
		return;

	/* TODO: save cursor position */

	/* scroll one line UP */
	//write(STDOUT_FILENO, "\033[1S", 4);

	/* scroll one line DOWN  */
	write(STDOUT_FILENO, "\033[1T", 4);

	/* set cursor position */
	/* Esc[Line;ColumnH */
	write(STDOUT_FILENO, "\033[0;0H", 6);

	for (l = bottom; l != NULL && rows > 0; l = TAILQ_NEXT(l, entries)) {
		rows -= l->len / cols + 1;
		//printf("rows: %d\n", rows);
	}

	if (l == NULL)
		return;

	write(STDOUT_FILENO, l->str, l->len);

	return;
}

int
main(int argc, char *argv[])
{
	TAILQ_INIT(&head);

	if (isatty(STDIN_FILENO) == 0)
		die("stdin it not a tty");
	if (isatty(STDOUT_FILENO) == 0)
		die("stdout it not a tty");

	if (argc <= 1)
		die("usage: scroll <program>");

	if (tcgetattr(STDIN_FILENO, &dfl) == -1)
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
	if ((f = fcntl(mfd, F_GETFL)) == -1)
		die("fcntl:");
	if (fcntl(mfd, F_SETFL, f /*| O_NONBLOCK*/) == -1)
		die("fcntl:");

	struct termios new = dfl;
	cfmakeraw(&new);
	new.c_cc[VMIN ] = 1;
	new.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &new) == -1)
		die("tcsetattr:");

	fd_set rd;
	size_t size = BUFSIZ, pos = 0;
	char *buf = calloc(size, sizeof *buf);
	if (buf == NULL)
		die("calloc:");
	memset(buf, 0, size);

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
			else if (write(mfd, &c, 1) == -1)
				die("write:");
		}
		if (FD_ISSET(mfd, &rd)) {
			if (read(mfd, &c, 1) <= 0 && errno != EINTR)
				die("read:");
			buf[pos++] = c;
			if (pos == size) {
				size *= 2;
				buf = realloc(buf, size);
				if (buf == NULL)
					die("realloc:");
			}
			if (c == '\n') {
				addline(buf, pos);
				memset(buf, 0, size);
				pos = 0;
			}
			if (write(STDOUT_FILENO, &c, 1) == -1)
				die("write:");
		}
	}
	return 0;
}
