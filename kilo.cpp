#include "lib/defer.h"
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

void disable_raw_mode(termios);
void enable_raw_mode(termios*);
char control_key(char c);

int main()
{
    termios origin_termios = {};
    enable_raw_mode(&origin_termios);
    defer { disable_raw_mode(origin_termios); };

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

    return 0;
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

char control_key(char c)
{
  return (c)&0x1f;
}

