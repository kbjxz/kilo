#include "lib/defer.h"
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <array>

void disable_raw_mode(termios);
void enable_raw_mode(termios*);
void read_and_print_loop();
constexpr char control_key(char c);
char read_key();
void process_key(char c);
void clear_screen();
#define die(v) panic(v, clear_screen)

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
            die("read");
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
        die("tcsetattr");
    }
}

void enable_raw_mode(termios* origin_termios)
{
    if (tcgetattr(STDIN_FILENO, origin_termios) == -1) {
        die("tcgetattr");
    }
    
    termios raw_mode = *origin_termios;
    raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_mode.c_oflag &= ~(OPOST);
    raw_mode.c_cflag |= (CS8);
    raw_mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_mode.c_cc[VMIN] = 0;
    raw_mode.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) == -1) {
        die("tcsetattr");
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
            die("read");
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

void clear_screen()
{
    static constexpr auto clear = std::to_array("\x1b[2J");
    write(STDOUT_FILENO, clear.data(), clear.size()-1);

    static constexpr auto reposition = std::to_array("\x1b[H");
    write(STDOUT_FILENO, reposition.data(), reposition.size()-1);
}