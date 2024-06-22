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
#define WAIT_TIME 4000000000
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (a) : (b))

void ctrl_w(
	int *const pos,
	int *error,
	struct timespec *last_back,
	const char *text)
{
	if (*error) {
		(*pos)++;
	}
	while (*pos > 0 && isspace(text[*pos - 1])) {
		(*pos)--;
	}
	while (*pos > 0 && !isspace(text[*pos - 1])) {
		(*pos)--;
	}
	*error = 0;
	clock_gettime(CLOCK_REALTIME, last_back);
}

void backspace(
	int *const pos,
	int *error,
	struct timespec *last_back,
	const char *text)
{
	if (*error) {
		(*pos)++;
	}
	while (*pos > 0 && !isalpha(text[*pos - 1])) {
		(*pos)--;
	}
	while (*pos > 0 && isalpha(text[*pos - 1])) {
		(*pos)--;
	}
	*error = 0;
	clock_gettime(CLOCK_REALTIME, last_back);
}

void sync_state(
	int sockfd,
	int pos,
	unsigned short *pos_to_word,
	short length,
	struct state *state_players,
	char *num_players)
{
	unsigned short current_word = pos_to_word[pos];

	struct state state = {
		.current_word = current_word,
	};

	send(sockfd, &state, sizeof state, 0);

	recv(sockfd, num_players, 1, 0);
	if (*num_players != 0) {
		recv(sockfd,
		     state_players,
		     *num_players * sizeof(struct state),
		     0);
	}
}

void input(
	int c,
	int sockfd,
	int *const pos,
	int *error,
	struct timespec last_back,
	struct timespec *last_input,
	struct state *state_players,
	char *num_players,
	short *pos_to_word,
	short length,
	const char *text)
{
	struct timespec current;
	clock_gettime(CLOCK_REALTIME, &current);
	long diff = (current.tv_sec - last_back.tv_sec) * 1000000000 +
		    current.tv_nsec - last_back.tv_nsec;

	if (*error || diff < WAIT_TIME) {
		return;
	}

	if (c == text[*pos]) {
		clock_gettime(CLOCK_REALTIME, last_input);

		if (isspace(text[*pos])) {
			sync_state(
				sockfd,
				*pos,
				pos_to_word,
				length,
				state_players,
				num_players);
		}
		(*pos)++;
	} else {
		*error = 1;
	}
}

void print_text(
	WINDOW *win,
	int pos,
	int error,
	struct timespec last_back,
	struct timespec last_input,
	struct state *state_players,
	char num_players,
	char const *text,
	unsigned short *pos_to_word,
	short length,
	short *spaces,
	short num_spaces)
{
	struct timespec current;
	clock_gettime(CLOCK_REALTIME, &current);
	long diff_back = (current.tv_sec - last_back.tv_sec) * 1000000000 +
			 current.tv_nsec - last_back.tv_nsec;
	long diff_input = (current.tv_sec - last_input.tv_sec) * 1000000000 +
			  current.tv_nsec - last_input.tv_nsec;

	unsigned short current_word = pos_to_word[pos];

	int space_start = current_word + AHEAD_HIDE < num_spaces
				  ? current_word + AHEAD_HIDE
				  : num_spaces - 1;
	int space_end = current_word + AHEAD_SHOW < num_spaces
				? current_word + AHEAD_SHOW
				: num_spaces - 1;
	int start = spaces[space_start];
	int end = spaces[space_end];

	wmove(win, 0, 0);
	wattron(win, A_DIM);
	for (int i = 0; i < MAX(spaces[current_word], 0); i++) {
		wprintw(win, "%c", text[i]);
	}
	wattroff(win, A_DIM);
	for (int i = MAX(spaces[current_word], 0); i < start; i++) {
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
			wprintw(win, " ");
		} else {
			wattron(win, A_BOLD);
			wprintw(win, "%c", text[i]);
			wattroff(win, A_BOLD);
		}
	}
	wattron(win, A_BOLD);
	for (int i = start; i < end; i++) {
		wprintw(win, "%c", text[i]);
	}
	wattroff(win, A_BOLD);
	for (int i = end; i < length; i++) {
		wprintw(win, "%c", text[i]);
	}

	for (int i = 0; i < num_players; i++) {
		wattron(win, COLOR_PAIR(2));
		int state_pos = spaces[state_players[i].current_word];
		if (state_pos >= 0) {
			mvwprintw(
				win,
				state_pos / 58,
				state_pos % 58,
				"%c",
				text[state_pos]);
		}
		wattroff(win, COLOR_PAIR(2));
	}

	for (int i = 0; i < num_players; i++) {
		mvprintw(20, 0, "pos = %d\n", state_players[i].current_word);
	}
	refresh();

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
		exit(1);
	}

	initscr();
	start_color();
	use_default_colors();
	noecho();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, -1, COLOR_RED);

	WINDOW *boxwin = newwin(15, 60, (LINES - 15) / 2, (COLS - 60) / 2);
	WINDOW *win = subwin(
		boxwin, 13, 58, (LINES - 15) / 2 + 1, (COLS - 60) / 2 + 1);

	struct state state_players[16];
	char num_players = 0;

	while (1) {
		struct timespec last_back = {0, 0};
		struct timespec last_input = {0, 0};
		int error = 0;

		wclear(boxwin);
		box(boxwin, 0, 0);
		mvwprintw(win, 0, 0, "Waiting text from server...");
		refresh();
		wrefresh(boxwin);
		wrefresh(win);

		char text[4096];
		int length = recv(sockfd, text, 4096, 0);
		text[length] = '\0';

		short spaces[1024];
		spaces[0] = -1;
		short num_spaces = 1;
		unsigned short pos_to_word[1024];
		for (int i = 0; i < length; i++) {
			pos_to_word[i] = num_spaces - 1;
			if (isspace(text[i])) {
				spaces[num_spaces] = i + 1;
				num_spaces++;
			}
		}

		spaces[num_spaces] = length;
		num_spaces++;

		int pos = 0;

		print_text(
			win,
			pos,
			error,
			last_back,
			last_input,
			state_players,
			num_players,
			text,
			pos_to_word,
			length,
			spaces,
			num_spaces);

		timeout(100);

		while (pos < length) {
			int c = getch();

			if (c == ERR) {
			} else if (c == 0x1b) {
				c = getch();
				if (c == '\b' || c == 0x7f) {
					backspace(
						&pos, &error, &last_back, text);
				}
			} else if (c == 0x17) {
				ctrl_w(&pos, &error, &last_back, text);
			} else {
				input(c,
				      sockfd,
				      &pos,
				      &error,
				      last_back,
				      &last_input,
				      state_players,
				      &num_players,
				      pos_to_word,
				      length,
				      text);
			}

			print_text(
				win,
				pos,
				error,
				last_back,
				last_input,
				state_players,
				num_players,
				text,
				pos_to_word,
				length,
				spaces,
				num_spaces);
		}
	}

	endwin();
	close(sockfd);
}
