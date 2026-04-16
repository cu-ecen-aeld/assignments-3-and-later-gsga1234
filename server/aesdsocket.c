/*
 * AESD Assignment 5 — TCP stream server on port 9000.
 * Packets are newline-terminated; each complete line is appended to
 * /var/tmp/aesdsocketdata, then the full file is sent back to the client.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT_STR "9000"
#define DATA_PATH "/var/tmp/aesdsocketdata"
#define RECV_CHUNK 256
#define SEND_CHUNK 4096
#define LINE_INITIAL 256

static volatile sig_atomic_t stop_requested;

static void on_signal(int signo)
{
	(void)signo;
	stop_requested = 1;
}

static int install_signals(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGINT, &sa, NULL) != 0)
		return -1;
	if (sigaction(SIGTERM, &sa, NULL) != 0)
		return -1;
	return 0;
}

static int append_packet_to_file(const char *data, size_t len)
{
	int fd = open(DATA_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) {
		syslog(LOG_ERR, "open %s for append: %s", DATA_PATH, strerror(errno));
		return -1;
	}

	size_t off = 0;
	while (off < len) {
		ssize_t w = write(fd, data + off, len - off);
		if (w < 0) {
			if (errno == EINTR) {
				if (stop_requested) {
					close(fd);
					return -1;
				}
				continue;
			}
			syslog(LOG_ERR, "write %s: %s", DATA_PATH, strerror(errno));
			close(fd);
			return -1;
		}
		off += (size_t)w;
	}
	close(fd);
	return 0;
}

static int send_entire_file(int connfd)
{
	int fd = open(DATA_PATH, O_RDONLY);
	if (fd < 0) {
		syslog(LOG_ERR, "open %s for read: %s", DATA_PATH, strerror(errno));
		return -1;
	}

	char buf[SEND_CHUNK];
	for (;;) {
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n < 0) {
			if (errno == EINTR) {
				if (stop_requested) {
					close(fd);
					return -1;
				}
				continue;
			}
			syslog(LOG_ERR, "read %s: %s", DATA_PATH, strerror(errno));
			close(fd);
			return -1;
		}
		if (n == 0)
			break;

		size_t sent = 0;
		while (sent < (size_t)n) {
			ssize_t w = send(connfd, buf + sent, (size_t)n - sent, 0);
			if (w < 0) {
				if (errno == EINTR) {
					if (stop_requested) {
						close(fd);
						return -1;
					}
					continue;
				}
				close(fd);
				return -1;
			}
			sent += (size_t)w;
		}
	}
	close(fd);
	return 0;
}

static int grow_line(char **line, size_t *cap, size_t len, size_t add)
{
	if (len + add + 1 > *cap) {
		size_t ncap = *cap ? *cap * 2 : LINE_INITIAL;
		while (ncap < len + add + 1)
			ncap *= 2;
		char *n = realloc(*line, ncap);
		if (!n) {
			syslog(LOG_ERR, "realloc line buffer failed (packet discarded)");
			free(*line);
			*line = NULL;
			*cap = 0;
			return -1;
		}
		*line = n;
		*cap = ncap;
	}
	return 0;
}

static int handle_client(int connfd, const char *peerhost)
{
	char *line = NULL;
	size_t linecap = 0;
	size_t linelen = 0;
	int rc = 0;

	while (!stop_requested) {
		char chunk[RECV_CHUNK];
		ssize_t n = recv(connfd, chunk, sizeof(chunk), 0);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			rc = -1;
			break;
		}
		if (n == 0)
			break;

		if (grow_line(&line, &linecap, linelen, (size_t)n) != 0) {
			linelen = 0;
			rc = -1;
			break;
		}
		memcpy(line + linelen, chunk, (size_t)n);
		linelen += (size_t)n;
		line[linelen] = '\0';

		char *start = line;
		for (;;) {
			char *nl = memchr(start, '\n', linelen - (size_t)(start - line));
			if (!nl)
				break;
			size_t pktlen = (size_t)(nl - start) + 1u;

			if (append_packet_to_file(start, pktlen) != 0) {
				rc = -1;
				goto out;
			}
			if (send_entire_file(connfd) != 0) {
				rc = -1;
				goto out;
			}

			size_t rest = linelen - (size_t)(nl + 1 - line);
			memmove(line, nl + 1, rest);
			linelen = rest;
			line[linelen] = '\0';
			start = line;
		}
	}

out:
	free(line);
	syslog(LOG_INFO, "Closed connection from %s", peerhost);
	return rc;
}

static int create_listening_socket(void)
{
	struct addrinfo hints, *res = NULL;
	int sockfd = -1;
	int yes = 1;
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	rv = getaddrinfo(NULL, PORT_STR, &hints, &res);
	if (rv != 0) {
		syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rv));
		return -1;
	}

	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		syslog(LOG_ERR, "socket: %s", strerror(errno));
		freeaddrinfo(res);
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
		syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %s", strerror(errno));
		close(sockfd);
		freeaddrinfo(res);
		return -1;
	}

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
		syslog(LOG_ERR, "bind port %s: %s", PORT_STR, strerror(errno));
		close(sockfd);
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);

	if (listen(sockfd, 16) != 0) {
		syslog(LOG_ERR, "listen: %s", strerror(errno));
		close(sockfd);
		return -1;
	}
	return sockfd;
}

static int run_daemon(void)
{
	pid_t pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "fork: %s", strerror(errno));
		return -1;
	}
	if (pid > 0)
		_exit(EXIT_SUCCESS);

	if (setsid() < 0) {
		syslog(LOG_ERR, "setsid: %s", strerror(errno));
		_exit(EXIT_FAILURE);
	}

	umask(0);
	if (chdir("/") < 0) {
		syslog(LOG_ERR, "chdir /: %s", strerror(errno));
		_exit(EXIT_FAILURE);
	}

	int fd0 = open("/dev/null", O_RDWR);
	if (fd0 >= 0) {
		dup2(fd0, STDIN_FILENO);
		dup2(fd0, STDOUT_FILENO);
		dup2(fd0, STDERR_FILENO);
		if (fd0 > 2)
			close(fd0);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	bool daemon_mode = false;
	int listenfd;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0)
			daemon_mode = true;
	}

	openlog("aesdsocket", LOG_PID, LOG_USER);

	listenfd = create_listening_socket();
	if (listenfd < 0) {
		closelog();
		return -1;
	}

	if (daemon_mode) {
		if (run_daemon() != 0) {
			close(listenfd);
			closelog();
			return -1;
		}
	}

	if (install_signals() != 0) {
		syslog(LOG_ERR, "sigaction failed");
		close(listenfd);
		closelog();
		return -1;
	}

	syslog(LOG_INFO, "aesdsocket listening on port %s", PORT_STR);

	while (!stop_requested) {
		struct sockaddr_storage peer_addr;
		socklen_t peer_len = sizeof(peer_addr);
		int connfd = accept(listenfd, (struct sockaddr *)&peer_addr, &peer_len);

		if (connfd < 0) {
			if (errno == EINTR) {
				if (stop_requested)
					break;
				continue;
			}
			syslog(LOG_ERR, "accept: %s", strerror(errno));
			break;
		}

		char peerhost[NI_MAXHOST];
		if (getnameinfo((struct sockaddr *)&peer_addr, peer_len,
				peerhost, sizeof(peerhost), NULL, 0,
				NI_NUMERICHOST) != 0) {
			strncpy(peerhost, "?", sizeof(peerhost) - 1);
			peerhost[sizeof(peerhost) - 1] = '\0';
		}

		syslog(LOG_INFO, "Accepted connection from %s", peerhost);
		handle_client(connfd, peerhost);
		close(connfd);
	}

	if (stop_requested)
		syslog(LOG_INFO, "Caught signal, exiting");

	close(listenfd);
	unlink(DATA_PATH);
	closelog();
	return 0;
}
