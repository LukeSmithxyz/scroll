/*
 * Based on an example code from Roberto E. Vargas Caballero.
 *
 * Copyright (c) 2020 Jan Klemkow <j.klemkow@wemelug.de>
 * Copyright (c) 2020 Jochen Sprickerhof <git@jochen.sprickerhof.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
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
	size_t size;
	size_t len;
	char *buf;
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

	exit(EXIT_FAILURE);
}

void
sigchld(int sig)
{
	pid_t pid;
	int status;

	assert(sig == SIGCHLD);

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		if (pid == child)
			exit(WEXITSTATUS(status));
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

/* error avoiding malloc */
void *
eamalloc(size_t size)
{
	void *mem;

	while ((mem = malloc(size)) == NULL) {
		struct line *line = TAILQ_LAST(&head, tailhead);

		if (line == NULL)
			die("malloc:");

		TAILQ_REMOVE(&head, line, entries);
		free(line->buf);
		free(line);
	}

	return mem;
}

/* error avoiding remalloc */
void *
earealloc(void *ptr, size_t size)
{
	void *mem;

	while ((mem = realloc(ptr, size)) == NULL) {
		struct line *line = TAILQ_LAST(&head, tailhead);

		if (line == NULL)
			die("realloc:");

		TAILQ_REMOVE(&head, line, entries);
		free(line->buf);
		free(line);
	}

	return mem;
}

/* Count string length w/o ansi esc sequences. */
size_t
strelen(const char *buf, size_t size)
{
	enum {CHAR, BREK, ESC} state = CHAR;
	size_t len = 0;

	for (size_t i = 0; i < size; i++) {
		char c = buf[i];

		switch (state) {
		case CHAR:
			if (c == '\033')
				state = BREK;
			else
				len++;
			break;
		case BREK:
			if (c == '[') {
				state = ESC;
			} else {
				state = CHAR;
				len++;
			}
			break;
		case ESC:
			if (c >= 64 && c <= 126)
				state = CHAR;
			break;
		}
	}

	return len;
}

/* alternate screen */
bool
isaltscreen(char c)
{
	static bool alt = false;
	static enum {CHAR, BREK, ESC} state = CHAR;
	static char buf[BUFSIZ];
	static size_t i = 0;

	switch (state) {
	case CHAR:
		if (c == '\033')
			state = BREK;
		break;
	case BREK:
		if (c == '[')
			state = ESC;
		else
			state = CHAR;
		break;
	case ESC:
		buf[i++] = c;
		if (i == sizeof buf) {
			/* TODO: find a better way to handle this situation */
			state = CHAR;
			i = 0;
		} else if (c >= 64 && c <= 126) {
			state = CHAR;
			buf[i] = '\0';
			i = 0;

			if (strcmp(buf, "?1049h") == 0 ||
			    strcmp(buf, "?1047h") == 0 ||
			    strcmp(buf, "?47h"  ) == 0 ||
			    strcmp(buf, "?1049l") == 0 ||
			    strcmp(buf, "?1047l") == 0 ||
			    strcmp(buf, "?47l"  ) == 0)
				alt = !alt;
		}
		break;
	}

	return alt;
}

void
addline(char *buf, size_t size)
{
	struct line *line = eamalloc(sizeof *line);

	line->size = size;
	line->len = strelen(buf, size);
	line->buf = eamalloc(size);
	memcpy(line->buf, buf, size);

	bottom = line;

	TAILQ_INSERT_HEAD(&head, line, entries);
}

void
scrollup(void)
{
	int rows = 0, start = 0;

	/* account for last line */
	if(TAILQ_PREV(bottom, tailhead, entries) == NULL)
		start = 1;

	/* wind back bottom pointer by two pages */
	for (; bottom != NULL && TAILQ_NEXT(bottom, entries) != NULL && rows < 2 * ws.ws_row; rows++)
		bottom = TAILQ_NEXT(bottom, entries);

	if (rows <= ws.ws_row) {
		bottom = TAILQ_LAST(&head, tailhead);
		return;
	}

	/* move the text in terminal n lines down */
	dprintf(STDOUT_FILENO, "\033[%dT", rows - ws.ws_row);
	/* set cursor position */
	write(STDOUT_FILENO, "\033[0;0H", 6);
	/* hide cursor */
	write(STDOUT_FILENO, "\033[?25l", 6);

	/* print one page */
	for (rows = 0; rows < ws.ws_row + start; rows++) {
		bottom = TAILQ_PREV(bottom, tailhead, entries);
		write(STDOUT_FILENO, bottom->buf, bottom->size);
	}
}

void
scrolldown(char *buf, size_t size)
{
	int rows = ws.ws_row;

	/* print one page */
	for (; rows > 0 && bottom != NULL && TAILQ_PREV(bottom, tailhead, entries) != NULL; rows--) {
		bottom = TAILQ_PREV(bottom, tailhead, entries);
		write(STDOUT_FILENO, bottom->buf, bottom->size);
	}
	if (rows < ws.ws_row && bottom == TAILQ_FIRST(&head)) {
		write(STDOUT_FILENO, "\033[?25h", 6);
		write(STDOUT_FILENO, buf, size);
	}
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

#ifdef __OpenBSD__
	if (pledge("stdio tty proc", NULL) == -1)
		die("pledge:");
#endif

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

	size_t size = BUFSIZ, pos = 0;
	char *buf = calloc(size, sizeof *buf);
	if (buf == NULL)
		die("calloc:");

	struct pollfd pfd[2] = {
		{STDIN_FILENO, POLLIN, 0},
		{mfd,          POLLIN, 0}
	};

	for (;;) {
		char c;

		if (poll(pfd, 2, -1) == -1 && errno != EINTR)
			die("poll:");

		if (pfd[0].revents & POLLIN) {
			if (read(STDIN_FILENO, &c, 1) <= 0 && errno != EINTR)
				die("read:");
			if (c == 17) /* ^Q */
				scrollup();
			else if (c == 18) /* ^R */
				scrolldown(buf, pos);
			else if (write(mfd, &c, 1) == -1)
				die("write:");
			else if (bottom != TAILQ_FIRST(&head)) {
				bottom = TAILQ_FIRST(&head);
				scrolldown(buf, pos);
			}
		}
		if (pfd[1].revents & POLLIN) {
			ssize_t n = read(mfd, &c, 1);
			if (n == -1 && errno != EINTR)
				die("read:");
			if (!isaltscreen(c)) {
				if (c == '\r') {
					addline(buf, pos);
					memset(buf, 0, size);
					pos = 0;
				}
				buf[pos++] = c;
				if (pos == size) {
					size *= 2;
					buf = earealloc(buf, size);
				}
			}
			if (write(STDOUT_FILENO, &c, 1) == -1)
				die("write:");
		}
	}

	return EXIT_SUCCESS;
}
