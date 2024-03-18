/*
    ttt-driver.c -- Driver program for Tic-Tac-Toe kernel module.

    Released into the public domain by Lawrence Sebald, 2020.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

static char respbuf[256];

#define UNK_ERROR   -1
#define OK          0
#define ILLMOVE     1
#define PLWIN       2
#define CPUWIN      3
#define TIEGAME     4

static ssize_t send_cmd(int fd, const char *cmd, size_t len) {
    ssize_t rs;

    /* Write the command. */
    if(write(fd, cmd, len) != len) {
        printf("Couldn't write command to /dev/tictactoe device: %s\n",
               strerror(errno));
        return -1;
    }

    /* Read the response. */
    if((rs = read(fd, respbuf, 256)) <= 0) {
        printf("Couldn't read response from /dev/tictactoe device: %s\n",
               strerror(errno));
        return -1;
    }

    return rs;
}

static int init_game(int fp, int which) {
    char cmd[5];
    ssize_t rs;

    cmd[0] = '0';
    cmd[1] = '0';
    cmd[2] = ' ';
    cmd[4] = '\n';

    if(which == 1)
        cmd[3] = 'X';
    else
        cmd[3] = 'O';

    /* Send the command and read the response. */
    rs = send_cmd(fp, cmd, 5);

    /* Make sure the response matches what we expect. */
    if(rs != 3 || memcmp(respbuf, "OK\n", 3))
        return -1;

    return 0;
}

static void read_move(int *x, int *y) {
    *x = *y = -1;

    while(*x < 0 || *x > 2) {
        printf("Enter the X coordinate of your move: ");
        scanf(" %d", x);
    }

    while(*y < 0 || *y > 2) {
        printf("Enter the Y coordinate of your move: ");
        scanf(" %d", y);
    }
}

static int print_board(int fp) {
    const char *cmd = "01\n";
    ssize_t rs;
    int i;
    char board[9];

    /* Send the command and read the response */
    rs = send_cmd(fp, cmd, 3);

    /* Make sure the response matches what we expect. */
    if(rs != 10 || respbuf[9] != '\n')
        return -1;

    /* Parse the board. */
    for(i = 0; i < 9; ++i) {
        if(respbuf[i] == 'X')
            board[i] = 'X';
        else if(respbuf[i] == 'O')
            board[i] = 'O';
        else if(respbuf[i] == '*')
            board[i] = ' ';
        else
            return -1;
    }

    /* Print it out. */
    for(i = 0; i < 3; ++i) {
        printf(" %c | %c | %c\n", board[i * 3], board[i * 3 + 1],
               board[i * 3 + 2]);
        if(i != 2)
            printf("-----------\n");
    }

    return 0;
}

static int request_cpu_move(int fp) {
    const char *cmd = "03\n";
    ssize_t rs;

    /* Send the command and read the response */
    rs = send_cmd(fp, cmd, 3);

    /* Figure out what the response was. */
    if(rs == 3 && !memcmp(respbuf, "OK\n", 3))
        return OK;
    else if(rs == 4 && !memcmp(respbuf, "WIN\n", 4))
        return CPUWIN;
    else if(rs == 4 && !memcmp(respbuf, "TIE\n", 4))
        return TIEGAME;
    else {
        printf("CPU Move returned unknown error code: ");
        printf("%.*s", rs, respbuf);
        return UNK_ERROR;
    }
}

static int make_move(int fp, int x, int y) {
    char cmd[7];
    ssize_t rs;

    cmd[0] = '0';
    cmd[1] = '2';
    cmd[2] = ' ';
    cmd[3] = (char)(x + '0');
    cmd[4] = ' ';
    cmd[5] = (char)(y + '0');
    cmd[6] = '\n';

    /* Send the command and read the response */
    rs = send_cmd(fp, cmd, 7);

    /* Figure out what the response was. */
    if(rs == 3 && !memcmp(respbuf, "OK\n", 3))
        return OK;
    else if(rs == 4 && !memcmp(respbuf, "WIN\n", 4))
        return PLWIN;
    else if(rs == 4 && !memcmp(respbuf, "TIE\n", 4))
        return TIEGAME;
    else if(rs == 8 && !memcmp(respbuf, "ILLMOVE\n", 8))
        return ILLMOVE;
    else {
        printf("Player Move returned unknown error code: ");
        printf("%.*s", rs, respbuf);
        return UNK_ERROR;
    }
}

static void exit_on_error(int fp, int err) {
    switch(err) {
        case PLWIN:
            printf("You Win!\n");
            close(fp);
            exit(EXIT_SUCCESS);

        case CPUWIN:
            printf("CPU Win...\n");
            close(fp);
            exit(EXIT_SUCCESS);

        case TIEGAME:
            printf("Game ends in a tie...\n");
            close(fp);
            exit(EXIT_SUCCESS);

        case UNK_ERROR:
            close(fp);
            exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    int which = -1;
    int rv, x, y, gameover = 0;
    int fd = open("/dev/tictactoe", O_RDWR);

    if(fd < 0) {
        printf("Cannot open /dev/tictactoe: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while(which != 1 && which != 2) {
        printf("Enter 1 to play as X, 2 to play as Y: ");
        scanf(" %d", &which);
    }

    /* Initialize the game board. */
    if(init_game(fd, which)) {
        close(fd);
        exit(EXIT_FAILURE);
    }

    for(;;) {
        if(which == 1) {
            rv = ILLMOVE;
            while(rv == ILLMOVE) {
                read_move(&x, &y);
                rv = make_move(fd, x, y);

                if(rv == ILLMOVE) {
                    printf("Illegal move, try again\n");
                }
            }

            exit_on_error(fd, rv);

            rv = print_board(fd);
            exit_on_error(fd, rv);

            printf("Requesting CPU move...\n");
            rv = request_cpu_move(fd);
            exit_on_error(fd, rv);

            rv = print_board(fd);
            exit_on_error(fd, rv);
        }
        else {
            printf("Requesting CPU move...\n");
            rv = request_cpu_move(fd);
            exit_on_error(fd, rv);

            rv = print_board(fd);
            exit_on_error(fd, rv);

            rv = ILLMOVE;
            while(rv == ILLMOVE) {
                read_move(&x, &y);
                rv = make_move(fd, x, y);

                if(rv == ILLMOVE) {
                    printf("Illegal move, try again\n");
                }
            }

            exit_on_error(fd, rv);

            rv = print_board(fd);
            exit_on_error(fd, rv);
        }
    }

    /* Never reached... */
    return 0;
}
