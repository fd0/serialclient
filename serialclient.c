/* vim:sw=4:sts=4:et:
   Copyright (C) Matthias Lederhofer <matled@gmx.net> {{{

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   this program; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
}}} */
/* include {{{*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <termio.h>
#include <getopt.h>
/*}}}*/

/* change also %nd for the bytecount printer if you change the buffer size */
#define BUF_SIZE 99

struct global_t
/*{{{*/ {
    struct termios restore_stdin;
    struct termios restore_serial;
    char *argv0;
    unsigned int stdin_failed : 1;
    int fd;
    /* options */
    char *path;
    int baudrate;
    long characters_per_line;
    unsigned int show_time : 1;
    unsigned int show_direction : 1;
    unsigned int show_bytecount : 1;
    unsigned int show_hex : 1;
    unsigned int show_decimal : 1;
    unsigned int show_binary : 1;
    unsigned int show_raw : 1;
    unsigned int show_printable_only : 1;
} global;
/*}}}*/

void usage(void);
void options(int argc, char *argv[]);
void cleanup(void);
void terminal_setup(void);
void terminal_restore(void);
void print(unsigned char *buf, size_t count, char prefix);

int main(int argc, char *argv[])
/*{{{*/ {
    global.argv0 = argv[0];
    global.fd = -1;
    global.stdin_failed = 0;

    options(argc, argv);

    if ((global.fd = open(global.path, O_RDWR)) == -1) {
        fprintf(stderr, "%s: Could not open \"%s\": %s\n",
            argv[0], global.path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    tcgetattr(STDIN_FILENO, &global.restore_stdin);
    tcgetattr(global.fd, &global.restore_serial);
    atexit(cleanup);
    terminal_setup();

    for (;;) {
        fd_set rfds;
        int retval, n;
        unsigned char buf[BUF_SIZE];

        FD_ZERO(&rfds);
        if (!global.stdin_failed) {
            FD_SET(STDIN_FILENO, &rfds);
        }
        FD_SET(global.fd, &rfds);
        retval = select(
            (global.stdin_failed || global.fd > STDIN_FILENO) ? global.fd+1 : STDIN_FILENO+1,
            &rfds, NULL, NULL, NULL);
        if (retval == -1) {
            fprintf(stderr, "%s: select failed: %s\n", argv[0], strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (retval == 0) {
            continue;
        }

        if (FD_ISSET(global.fd, &rfds)) { /*{{{*/
            n = read(global.fd, buf, sizeof(buf));
            if (n == 0) {
                fprintf(stderr, "%s: read from device returned 0 bytes (terminating)\n",
                    global.argv0);
                exit(EXIT_SUCCESS);
            } else if (n == -1) {
                if (errno != EAGAIN) {
                    fprintf(stderr, "%s: read from device failed: %s\n", global.argv0, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            } else {
                print(buf, n, '<');
            }
        } /*}}}*/

        if (!global.stdin_failed && FD_ISSET(STDIN_FILENO, &rfds)) /*{{{*/ {
            n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n == -1) {
                if (errno != EAGAIN) {
                    global.stdin_failed = 1;
                }
            } else if (n == 0) {
                global.stdin_failed = 1;
            } else {
                int i = write(global.fd, buf, n);
                if (i != n) {
                    if (i ==  -1) {
                        fprintf(stderr, "%s: write to device failed: %s\n",
                            global.argv0, strerror(errno));
                    } else {
                        fprintf(stderr, "%s: could not write all data to "
                            "device (terminating)\n",
                            global.argv0);
                    }
                    exit(EXIT_FAILURE);
                }
                print(buf, n, '>');
            }
        } /*}}}*/
    }
} /*}}}*/

void usage(void)
/*{{{*/ {
    printf("Usage: %s [OPTION]... DEVICE\n",
        global.argv0);
    printf(
        "  -r  --baudrate      set baudrate\n"
        "  -n  --characters-per-line  number of characters per line\n"
        "  -t  --time          display time\n"
        "  -T  --no-time       do not display time\n"
        "  -p  --direction     display `>' for transmitted and `<' for received data\n"
        "  -P  --no-direction  display `>' for transmitted and `<' for received data\n"
        "  -c  --count         display number of bytes received/sent\n"
        "  -C  --no-count      do not display number of bytes received/sent\n"
        "  -x  --hex           show data in hex\n"
        "  -X  --no-hex        do not show data hex\n"
        "  -d  --decimal       show data in decimal\n"
        "  -D  --no-decimal    do not show data in decimal\n"
        "  -b  --binary        show data in binary (0/1)\n"
        "  -B  --no-binary     do not show data binary (0/1)\n"
        "  -a  --ascii         show data in ascii\n"
        "  -A  --no-ascii      do not show data ascii\n"
        "  -u  --unsafe        do print all characters for --ascii\n"
        "  -U  --no-unsafe     do not print all characters for --ascii (only 0x21 to 0x7E)\n"
        "  -h, --help          display this help and exit\n");
} /*}}}*/

void options(int argc, char *argv[])
/*{{{*/ {
    int error = 0;
    int c;
    const struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"baudrate", required_argument, 0, 'r'},
        {"characters-per-line", required_argument, 0, 'n'},
        {"time", no_argument, 0, 't'},
        {"no-time", no_argument, 0, 'T'},
        {"direction", no_argument, 0, 'p'},
        {"no-direction", no_argument, 0, 'P'},
        {"count", no_argument, 0, 'c'},
        {"no-count", no_argument, 0, 'C'},
        {"hex", no_argument, 0, 'x'},
        {"no-hex", no_argument, 0, 'X'},
        {"decimal", no_argument, 0, 'd'},
        {"no-decimal", no_argument, 0, 'D'},
        {"binary", no_argument, 0, 'b'},
        {"no-binary", no_argument, 0, 'B'},
        {"ascii", no_argument, 0, 'a'},
        {"no-ascii", no_argument, 0, 'A'},
        {"unsafe", no_argument, 0, 'u'},
        {"no-unsafe", no_argument, 0, 'U'},
        {0, 0, 0, 0}
    };

    /* set default values */
    global.baudrate = B19200;
    global.characters_per_line = 8;
    global.show_time = 1;
    global.show_direction = 1;
    global.show_bytecount = 1;
    global.show_hex = 1;
    global.show_decimal = 0;
    global.show_binary = 0;
    global.show_raw = 1;
    global.show_printable_only = 1;

    /* evaluate arguments */
    while ((c = getopt_long(argc, argv, "hr:n:tTpPcCxXdDbBaAuU", longopts, 0)) != -1) {
    switch(c) {
    case 'h':
        usage();
        exit(EXIT_SUCCESS);
    case 'r':
        switch (atoi(optarg)) {
        case 50:     global.baudrate = B50; break;
        case 75:     global.baudrate = B75; break;
        case 110:    global.baudrate = B110; break;
        case 134:    global.baudrate = B134; break;
        case 150:    global.baudrate = B150; break;
        case 200:    global.baudrate = B200; break;
        case 300:    global.baudrate = B300; break;
        case 600:    global.baudrate = B600; break;
        case 1200:   global.baudrate = B1200; break;
        case 1800:   global.baudrate = B1800; break;
        case 2400:   global.baudrate = B2400; break;
        case 4800:   global.baudrate = B4800; break;
        case 9600:   global.baudrate = B9600; break;
        case 19200:  global.baudrate = B19200; break;
        case 38400:  global.baudrate = B38400; break;
        case 57600:  global.baudrate = B57600; break;
        case 115200: global.baudrate = B115200; break;
        case 230400: global.baudrate = B230400; break;
        default:
            fprintf(stderr, "%s: invalid baudrate %s\n",
                global.argv0, optarg);
            error = 1;
            break;
        }
        break;
    case 'n':
        global.characters_per_line = strtol(optarg, NULL, 0);
        if (global.characters_per_line == LONG_MIN || global.characters_per_line == LONG_MAX) {
            fprintf(stderr, "%s: invalid number of characters per line %s: %s\n",
                global.argv0, optarg, strerror(errno));
            error = 1;
        }
        if (global.characters_per_line < 0) {
            fprintf(stderr, "%s: invalid number of characters per line %s: has to be at least zero\n",
                global.argv0, optarg);
            error = 1;
        }
        if (global.characters_per_line == 0) {
            global.characters_per_line = BUF_SIZE;
        }
        break;
    case 't': global.show_time = 1; break;
    case 'T': global.show_time = 0; break;
    case 'p': global.show_direction = 1; break;
    case 'P': global.show_direction = 0; break;
    case 'c': global.show_bytecount = 1; break;
    case 'C': global.show_bytecount = 0; break;
    case 'x': global.show_hex = 1; break;
    case 'X': global.show_hex = 0; break;
    case 'd': global.show_decimal = 1; break;
    case 'D': global.show_decimal = 0; break;
    case 'b': global.show_binary = 1; break;
    case 'B': global.show_binary = 0; break;
    case 'a': global.show_raw = 1; break;
    case 'A': global.show_raw = 0; break;
    case 'u': global.show_printable_only = 0; break;
    case 'U': global.show_printable_only = 1; break;
    case '?': error = 1; break;
    }}
    if (optind != argc-1) {
        fprintf(stderr, "%s: give one device after options\n", global.argv0);
        error = 1;
    }
    if (error) {
        fprintf(stderr, "Try `%s --help' for more information.\n",
            global.argv0);
        exit(EXIT_FAILURE);
    }
    global.path = argv[optind];
} /*}}}*/

void cleanup(void)
/*{{{*/ {
    terminal_restore();
    if (global.fd != -1) {
        close(global.fd);
    }
} /*}}}*/

void terminal_setup(void)
/*{{{*/ {
    struct termios attr;

    /* stdin: disable line buffer, local echo */
    memcpy(&attr, &global.restore_stdin, sizeof(struct termios));
    attr.c_lflag &= ~ICANON;
    attr.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);

    /* serial */
    tcgetattr(global.fd, &attr);

    /* set baudrate, 8n1, as-raw-as-possible ... */
    attr.c_cflag = CS8 | CREAD | CLOCAL;
    attr.c_lflag = 0;

    attr.c_iflag = IGNBRK | IMAXBEL;
    attr.c_oflag = 0;

    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 5;

    cfsetispeed(&attr, global.baudrate);
    cfsetospeed(&attr, global.baudrate);

    int mcs = 0;
    ioctl(global.fd, TIOCMGET, &mcs);
    mcs |= TIOCM_RTS;
    ioctl(global.fd, TIOCMSET, &mcs);

    tcsetattr(global.fd, TCSANOW, &attr);
} /*}}}*/

void terminal_restore(void)
/*{{{*/ {
    tcsetattr(STDIN_FILENO, 0, &global.restore_stdin);
    if (global.fd != -1)
        tcsetattr(global.fd, 0, &global.restore_serial);
} /*}}}*/

void print(unsigned char *buf, size_t count, char prefix)
/*{{{*/ {
    size_t padlen = 0;

    if (global.show_time) {
        struct timeval now;
        struct tm *now_tm;
        char now_str[128];
        if (gettimeofday(&now, NULL) == -1) {
            fprintf(stderr, "%s: gettimeofday failed: %s\n", global.argv0, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if ((now_tm = localtime(&now.tv_sec)) == NULL) {
            fprintf(stderr, "%s: localtime failed: %s\n", global.argv0, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (strftime(now_str, sizeof(now_str), "%H:%M:%S", now_tm) == 0) {
            fprintf(stderr, "%s: strftime returned 0\n", global.argv0);
            exit(EXIT_FAILURE);
        }
        int n = printf("%s.%06lu ", now_str, now.tv_usec);
        if (n > 0) padlen += (size_t)n;
    }
    if (global.show_direction) {
        int n = printf("%c ", prefix);
        if (n > 0) padlen += (size_t)n;
    }
    if (global.show_bytecount) {
        int n = printf("%2d: ", count);
        if (n > 0) padlen += (size_t)n;
    }
    
    for (size_t i = 0; i < count; i += global.characters_per_line) {
        if (i != 0) {
            for (size_t j = 0; j < padlen; ++j) {
                putchar(' ');
            }
        }
        int first = 1;
        /* hex */
        if (global.show_hex) {
            if (!first) printf("| ");
            first = 0;
            for (size_t j = i; j < i+global.characters_per_line; ++j) {
                if (j < count) printf("%02X ", buf[j]);
                else           printf("   ");
            }
        }
        /* decimal */
        if (global.show_decimal) {
            if (!first) printf("| ");
            first = 0;
            for (size_t j = i; j < i+global.characters_per_line; ++j) {
                if (j < count) printf("%3d ", buf[j]);
                else           printf("    ");
            }
        }
        /* binary */
        if (global.show_binary) {
            if (!first) printf("| ");
            first = 0;
            for (size_t j = i; j < i+global.characters_per_line; ++j) {
                if (j < count) {
                    printf("%d%d%d%d%d%d%d%d ",
                        (buf[j] >> 7) & 1,
                        (buf[j] >> 6) & 1,
                        (buf[j] >> 5) & 1,
                        (buf[j] >> 4) & 1,
                        (buf[j] >> 3) & 1,
                        (buf[j] >> 2) & 1,
                        (buf[j] >> 1) & 1,
                        (buf[j] >> 0) & 1);
                } else {
                    printf("         ");
                }
            }
        }
        /* ascii */
        if (global.show_raw) {
            if (!first) printf("| ");
            first = 0;
            for (size_t j = i; j < i+global.characters_per_line && j < count; ++j) {
                printf("%c",
                    !global.show_printable_only || (buf[j] > 0x20 && buf[j] < 0x7F)
                        ? buf[j] : '.');
            }
        }
        printf("\n");
    }
} /*}}}*/
