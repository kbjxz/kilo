#include "lib/defer.h"
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

void disable_raw_mode(termios);
void enable_raw_mode(termios*);
void read_and_print_loop();
constexpr char control_key(char c);
char read_key();
void process_key(char c);

int main()
{
    termios origin_termios = {};
    enable_raw_mode(&origin_termios);
    defer { disable_raw_mode(origin_termios); };

    for (;;) {
        char c = read_key();
        process_key(c);
    } 

    return 0;
}

void read_and_print_loop()
{
    for (;;) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1) {
            panic("read failed");
        }
        if (iscntrl(c)) {
            printf("[ctrl] %d\r\n", c);
        } else if (c == 'q') {
            break;
        } else {
            printf("[char] %d (%c)\r\n", c, c);
        }
    }
}

void disable_raw_mode(termios t)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1) {
        panic("tcsetattr");
    }
}

void enable_raw_mode(termios* origin_termios)
{
    if (tcgetattr(STDIN_FILENO, origin_termios) == -1) {
        panic("tcgetattr");
    }
    
    termios raw_mode = *origin_termios;
    raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_mode.c_oflag &= ~(OPOST);
    raw_mode.c_cflag |= (CS8);
    raw_mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_mode.c_cc[VMIN] = 0;
    raw_mode.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) == -1) {
        panic("tcsetattr");
    }
}

constexpr char control_key(char c) 
{
  return (c)&0x1f;
}

char read_key()
{
    int n;
    char c;
    for (;;) {
        n = read(STDIN_FILENO, &c, 1);
        if (n != 1) {
            continue;
        }
        if (n == -1 &&  errno != EAGAIN) {
            panic("read");
        }
        break;
    }
    return c;
}

void process_key(char c)
{
    switch (c) {
        case control_key('q'):
            exit(0);
            break;
    }
}