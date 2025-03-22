#include "tt.h"
#include <json.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int change_quote(
	int quote_idx,
	struct json_object *quotes,
	struct state states[FD_SETSIZE],
	int sockfd,
	fd_set set)
{
	json_object *quote = json_object_array_get_idx(quotes, quote_idx);
	json_object_object_get_ex(quote, "text", &quote);
	const char *str_quote = json_object_get_string(quote);
	short length = strlen(str_quote);
	for (int i = 0; i < FD_SETSIZE; i++) {
		states[i].pos = 0;
		if (!(i != 0 && i != sockfd && FD_ISSET(i, &set))) {
			continue;
		}
		send(i, "\x00", 1, 0);
		send(i, &length, sizeof length, 0);
		send(i, str_quote, length, 0);
	}
}

int main(int argc, const char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <port> <quotes.json>\n", argv[0]);
		return 1;
	}

	char *endptr;
	long port = strtol(argv[1], &endptr, 10);
	if (!(*argv[1] != '\0' && *endptr == '\0' && port < 65536)) {
		fprintf(stderr, "ERROR: '%s' is not a valid port.\n", argv[1]);
		return 1;
	}

	struct json_object *root = json_object_from_file(argv[2]);
	if (!root) {
		fprintf(stderr, "Failed to parse JSON file.\n");
		return 1;
	}
	struct json_object *quotes;
	json_object_object_get_ex(root, "quotes", &quotes);
	int num_quotes = json_object_array_length(quotes);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = INADDR_ANY};
	if (bind(sockfd, (struct sockaddr *)&address, sizeof address) == -1) {
		perror("bind");
		return 1;
	}
	if (listen(sockfd, 5) == -1) {
		perror("listen");
		return 1;
	}

	fd_set set;
	FD_ZERO(&set);
	FD_SET(0, &set);
	FD_SET(sockfd, &set);

	struct state states[FD_SETSIZE];

	while (1) {
		fd_set readfds = set;
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) == -1) {
			perror("select");
			return 1;
		}

		if (FD_ISSET(0, &readfds)) {
			char buffer[4096];
			read(0, buffer, 4096);
			long quote_idx = strtol(buffer, &endptr, 10);
			if (!((*buffer != '\n' && *buffer != '\0') &&
			      (*endptr == '\0' || *endptr == '\n'))) {
				fprintf(stderr, "Error parsing quote index.\n");
			} else if (!(0 <= quote_idx &&
				     quote_idx < num_quotes)) {
				fprintf(stderr,
					"0 <= quote_index < %d.\n",
					num_quotes);
			} else {
				change_quote(
					quote_idx, quotes, states, sockfd, set);
			}
		}

		if (FD_ISSET(sockfd, &readfds)) {
			int fd = accept(sockfd, NULL, NULL);
			FD_SET(fd, &set);
			states[fd].pos = 0;
			printf("player %d connected\n", fd);
		}

		for (int fd = 0; fd < FD_SETSIZE; fd++) {
			if (!(fd != 0 && fd != sockfd &&
			      FD_ISSET(fd, &readfds))) {
				continue;
			}

			struct {
				char type;
				unsigned int number;
			} message;

			int length = recv(fd, &message, 1 + sizeof(int), 0);

			if (length == 0) {
				states[fd].pos = 0;
				printf("Disconnected\n");
				FD_CLR(fd, &set);
				continue;
			}

			if (message.type == 0) {
				states[fd].pos = message.number;
				for (int dest = 0; dest < FD_SETSIZE; dest++) {
					if (!(dest != 0 && dest != fd &&
					      dest != sockfd &&
					      FD_ISSET(dest, &set))) {
						continue;
					}
					struct state buffer[FD_SETSIZE];
					unsigned short buffer_size = 0;
					for (int i = 0; i < FD_SETSIZE; i++) {
						if (!(i != 0 && i != dest &&
						      i != sockfd &&
						      FD_ISSET(i, &set))) {
							continue;
						}
						buffer[buffer_size++] =
							states[i];
					}
					buffer_size *= sizeof(struct state);

					send(dest, "\x01", 1, 0);
					send(dest, &buffer_size, 2, 0);
					send(dest, buffer, buffer_size, 0);
				}
			}

			if (message.type == 1) {
				int quote_idx = message.number;
				change_quote(
					quote_idx, quotes, states, sockfd, set);
			}
		}
	}
	return 0;
}
