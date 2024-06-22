#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "tt.h"
#include <stdlib.h>
#include <json-c/json.h>
#include <string.h>

int main(int argc, const char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		return 1;
	}

	char* endptr;
	long port = strtol(argv[1], &endptr, 10);
	if (!(*argv[1] != '\0' && *endptr == '\0' && port < 65536)) {
		fprintf(stderr, "ERROR: '%s' is not a valid port.\n", argv[1]);
		return 1;
	}

	struct json_object *root = json_object_from_file("data/quotes_english.json");
	if (!root) {
		fprintf(stderr, "Failed to parse JSON file.\n");
		return 1;
	}
	struct json_object *quotes;
	json_object_object_get_ex(root, "quotes", &quotes);
	size_t num_quotes = json_object_array_length(quotes);


	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = INADDR_ANY
	};
	if (bind(sockfd, (struct sockaddr*) &address, sizeof address) == -1){
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

	char running = 0;

	printf("\nChoose quote index: ");
	fflush(stdout);
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
			if (!((*buffer != '\n' && *buffer != '\0') && (*endptr == '\0' || *endptr =='\n'))) {
				fprintf(stderr, "Error parsing quote index.\n");
			} else {
				json_object *quote = json_object_array_get_idx(quotes, quote_idx);
				json_object_object_get_ex(quote, "text", &quote);
				const char *str_quote = json_object_get_string(quote);
				for (int i = 0; i < FD_SETSIZE; i++) {
					states[i].pos = 0;
					if (!(i != 0 && i != sockfd && FD_ISSET(i, &set))) {
						continue;
					}
					send(i, str_quote, strlen(str_quote), 0);
				}
			}
			printf("\nChoose quote index: ");
			fflush(stdout);
			
		}

		if (FD_ISSET(sockfd, &readfds)) {
			int fd = accept(sockfd, NULL, NULL);
			FD_SET(fd, &set);
			printf("player %d connected\n", fd);
		}

		for (int fd = 0; fd < FD_SETSIZE; fd++) {
			if (!(fd != 0 && fd != sockfd && FD_ISSET(fd, &readfds))) {
				continue;
			}

			int length = recv(fd, &states[fd], 16, 0);
			if (length == 0) {
				printf("Disconnected\n");
				FD_CLR(fd, &set);
				continue;
			}

			struct state buffer[FD_SETSIZE];
			unsigned short buffer_size = 0;
			for (int i = 0; i < FD_SETSIZE; i++) {
				if (!(i != 0 && i != fd && i != sockfd && FD_ISSET(i, &set))) {
					continue;
				}
				buffer[buffer_size++] = states[i];
			}
			send(fd, &buffer_size, 1, 0);
			send(fd, buffer, buffer_size * (sizeof (struct state)), 0);
		}

	}
	return 0;
}
