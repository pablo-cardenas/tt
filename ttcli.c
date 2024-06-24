#include "tt.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define AHEAD_SHOW 4
#define AHEAD_HIDE 2
#define WAIT_TIME 1500000000
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

void backspace(
	int *const pos,
	int *error,
	struct timespec *last_back,
	const char *text,
	int (*func)(int),
	int flip)
{
	if (*error) {
		(*pos)++;
	}
	while (*pos > 0 && (flip ^ !func(text[*pos - 1]))) {
		(*pos)--;
	}
	while (*pos > 0 && (flip ^ (!!func(text[*pos - 1])))) {
		(*pos)--;
	}
	*error = 0;
	clock_gettime(CLOCK_REALTIME, last_back);
}

void print_text(
	WINDOW *win,
	int pos,
	int error,
	int count_errors,
	int count_slows,
	struct timespec last_back,
	struct timespec last_input,
	struct timespec first_input,
	struct state *state_players,
	char num_players,
	char const *text,
	unsigned short *pos_to_word,
	short length,
	short *spaces,
	short num_spaces)
{
	wclear(win);
	float elapsed =
		(last_input.tv_sec - first_input.tv_sec) +
		(last_input.tv_nsec - first_input.tv_nsec) / 1000000000.0;

	mvwprintw(win, 9, 24, "   WPM: %.0f", (pos / 5.0) / (elapsed / 60.0));
	mvwprintw(win, 10, 24, "Errors: %d", count_errors);
	mvwprintw(win, 11, 24, " Slows: %d", count_slows);

	struct timespec current;
	clock_gettime(CLOCK_REALTIME, &current);

	long diff_back = (current.tv_sec - last_back.tv_sec) * 1000000000 +
			 current.tv_nsec - last_back.tv_nsec;
	long diff_input = (current.tv_sec - last_input.tv_sec) * 1000000000 +
			  current.tv_nsec - last_input.tv_nsec;

	unsigned short current_word = pos_to_word[pos];

	int space_start = MIN(current_word + AHEAD_HIDE, num_spaces - 1);
	int space_end = MIN(current_word + AHEAD_SHOW, num_spaces - 1);

	wmove(win, 0, 0);
	for (int i = 0; i < length; i++) {
		if (pos >= length) {
			wprintw(win, "%c", text[i]);
		} else if (i < spaces[current_word]) {
			wattron(win, A_DIM);
			wprintw(win, "%c", text[i]);
			wattroff(win, A_DIM);
		} else if (i < spaces[space_start]) {
			if (error) {
				wattron(win, COLOR_PAIR(2));
				wprintw(win, "%c", text[i]);
				wattroff(win, COLOR_PAIR(2));
			} else if (diff_back < WAIT_TIME) {
				wattron(win, A_BOLD);
				wattron(win, COLOR_PAIR(1));
				wprintw(win, "%c", text[i]);
				wattroff(win, COLOR_PAIR(1));
				wattroff(win, A_BOLD);
			} else if (diff_input < WAIT_TIME) {
				if (isalpha(text[i])) {
					wprintw(win, " ");
				} else {
					wprintw(win, "%c", text[i]);
				}
			} else {
				/* wattron(win, A_BOLD); */
				wprintw(win, "%c", text[i]);
				/* wattroff(win, A_BOLD); */
			}
		} else if (i < spaces[space_end]) {
			wattron(win, A_BOLD);
			wprintw(win, "%c", text[i]);
			wattroff(win, A_BOLD);
		} else {
			wprintw(win, "%c", text[i]);
		}
	}

	for (int i_player = 0; i_player < num_players; i_player++) {
		int i = state_players[i_player].pos;

		int y = i / 58;
		int x = i % 58;

		int c = text[i] == ' ' ? '_' : text[i];

		wattron(win, COLOR_PAIR(3));
		if (i < spaces[current_word]) {
			wattron(win, A_DIM);
			mvwprintw(win, y, x, "%c", c);
			wattroff(win, A_DIM);
		} else if (i < spaces[space_start]) {
			if (error) {
				wattron(win, COLOR_PAIR(4));
				mvwprintw(win, y, x, "%c", c);
				wattroff(win, COLOR_PAIR(4));
			} else if (diff_back < WAIT_TIME) {
				wattron(win, A_BOLD);
				mvwprintw(win, y, x, "%c", c);
				wattroff(win, A_BOLD);
			} else if (diff_input < WAIT_TIME) {
				mvwprintw(win, y, x, "_");
			} else {
				/* wattron(win, A_BOLD); */
				mvwprintw(win, y, x, "%c", c);
				/* wattroff(win, A_BOLD); */
			}
		} else if (i < spaces[space_end]) {
			wattron(win, A_BOLD);
			mvwprintw(win, y, x, "%c", c);
			wattroff(win, A_BOLD);
		} else {
			mvwprintw(win, y, x, "%c", c);
		}
		wattroff(win, COLOR_PAIR(3));
	}

	if (error) {
		wmove(win, (pos + 1) / 58, (pos + 1) % 58);
	} else {
		wmove(win, pos / 58, pos % 58);
	}

	wrefresh(win);
}

int main(int argc, const char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
		return 1;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
	};

	int s;
	if ((s = inet_pton(AF_INET, argv[1], &addr.sin_addr)) != 1) {
		if (s == 0) {
			fprintf(stderr, "Host is not in presentation format\n");
		} else {
			perror("host");
		}
		return 1;
	}

	char *endptr;
	long port = strtol(argv[2], &endptr, 10);
	if (!(*argv[2] != '\0' && *endptr == '\0' && port < 65536)) {
		fprintf(stderr, "ERROR: '%s' not a valid port.\n", argv[2]);
		return 1;
	}
	addr.sin_port = htons(port);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int status = connect(sockfd, (struct sockaddr *)&addr, sizeof addr);
	if (status == -1) {
		perror("connect");
		endwin();
		return 1;
	}

	initscr();
	start_color();
	use_default_colors();
	noecho();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, -1, COLOR_RED);
	init_pair(3, COLOR_CYAN, -1);
	init_pair(4, COLOR_CYAN, COLOR_RED);

	WINDOW *boxwin = newwin(15, 60, (LINES - 15) / 2, (COLS - 60) / 2);
	WINDOW *win = subwin(
		boxwin, 13, 58, (LINES - 15) / 2 + 1, (COLS - 60) / 2 + 1);

	wclear(boxwin);
	box(boxwin, 0, 0);
	refresh();
	wrefresh(boxwin);

	struct state state_players[16];
	char num_players = 0;

	/*
	 * if recv_step == 0: read_sock - Waiting text from server.
	 * if recv_step == 1: read sock - Pending to receive num_players.
	 * if recv_step == 2: read sock - Pending to receive state_players.
	 */
	char recv_step = 0;
	char outdated = 0;

	char header_type;
	short header_length;

	int pos;
	int error;
	int count_errors = 0;
	int count_slows = 0;

	char text[4096];
	int length;

	short spaces[1024];
	short num_spaces;
	unsigned short pos_to_word[1024];

	struct timespec last_back = {0, 0};
	struct timespec last_input = {0, 0};
	struct timespec first_input = {0, 0};

	while (1) {
		fd_set readfds, writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(0, &readfds);
		FD_SET(sockfd, &readfds);
		if (outdated == 1) {
			FD_SET(sockfd, &writefds);
		}

		struct timeval select_timeout = {1, 200000};
		select(FD_SETSIZE, &readfds, &writefds, NULL, &select_timeout);

		if (FD_ISSET(0, &readfds)) {
			if (first_input.tv_sec == 0 &&
			    first_input.tv_nsec == 0) {
				clock_gettime(CLOCK_REALTIME, &first_input);
			}

			int c = getch();

			if (c == ERR) {
			} else if (c == 0x1b) {
				c = getch();
				if (c == '\b' || c == 0x7f) {
					backspace(
						&pos,
						&error,
						&last_back,
						text,
						isalpha,
						0);
					outdated = 1;
				}
			} else if (c == 0x17) {
				backspace(
					&pos,
					&error,
					&last_back,
					text,
					isspace,
					1);
				outdated = 1;
			} else {
				struct timespec current;
				clock_gettime(CLOCK_REALTIME, &current);
				long diff_back =
					(current.tv_sec - last_back.tv_sec) *
						1000000000 +
					current.tv_nsec - last_back.tv_nsec;
				long diff_input =
					(current.tv_sec - last_input.tv_sec) *
						1000000000 +
					current.tv_nsec - last_input.tv_nsec;

				if (pos < length && !error &&
				    diff_back > WAIT_TIME) {
					if (c == text[pos]) {
						if (pos != 0 &&
						    diff_input > WAIT_TIME) {
							count_slows++;
						}
						last_input = current;
						pos++;
						outdated = 1;
					} else {
						error = 1;
						count_errors++;
					}
				}
			}
		}

		if (FD_ISSET(sockfd, &readfds)) {
			int buffer_length;
			void *buffer;
			switch (recv_step) {
			case 0:
				// TYPE
				buffer_length = 1;
				buffer = &header_type;
				break;
			case 1:
				// LENGTH
				buffer_length = 2;
				buffer = &header_length;
				break;
			case 2:
				// MESSAGE
				buffer_length = header_length;
				if (header_type == 0) {
					buffer = &text;
				} else if (header_type == 1) {
					buffer = &state_players;
				}
				break;
			}

			int recv_length =
				recv(sockfd, buffer, buffer_length, 0);

			if (recv_length == 0) {
				endwin();
				fprintf(stderr, "Closed by server\n");
				close(sockfd);
				return 0;
			}

			switch (recv_step) {
			case 0:
				// MESSAGE
				recv_step = 1;
				break;
			case 1:
				// LENGTH
				if (header_length == 0) {
					recv_step = 0;
				} else {
					recv_step = 2;
				}
				break;

			case 2:
				// MESSAGE
				recv_step = 0;
				if (header_type == 0) {
					length = recv_length;
					text[length] = '\0';

					memset(&last_back, 0, sizeof last_back);
					memset(&last_input,
					       0,
					       sizeof last_input);
					memset(&first_input,
					       0,
					       sizeof first_input);

					error = 0;
					count_errors = 0;
					count_slows = 0;

					spaces[0] = -1;
					num_spaces = 1;
					for (int i = 0; i < length; i++) {
						pos_to_word[i] = num_spaces - 1;
						if (isspace(text[i])) {
							spaces[num_spaces] =
								i + 1;
							num_spaces++;
						}
					}
					spaces[num_spaces] = length;
					num_spaces++;

					pos = 0;
					num_players = 0;
				} else if (header_type == 1) {
					num_players = header_length /
						      (sizeof(struct state));
				}
				break;
			}
		}

		if (FD_ISSET(sockfd, &writefds)) {
			struct state state = {
				.pos = pos,
			};
			send(sockfd, &state, sizeof state, 0);
			outdated = 0;
		}

		print_text(
			win,
			pos,
			error,
			count_errors,
			count_slows,
			last_back,
			last_input,
			first_input,
			state_players,
			num_players,
			text,
			pos_to_word,
			length,
			spaces,
			num_spaces);
	}

	endwin();
	close(sockfd);
}
