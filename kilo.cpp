#include "lib/defer.h"
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <array>
#include <sys/ioctl.h>

void clear_screen()
{
    static constexpr auto clear = std::to_array("\x1b[2J");
    write(STDOUT_FILENO, clear.data(), clear.size()-1);

    static constexpr auto reposition = std::to_array("\x1b[H");
    write(STDOUT_FILENO, reposition.data(), reposition.size()-1);
}

#define die(v) panic(v, clear_screen)

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


void disable_raw_mode(const termios t)
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


result<winsize> get_window_size()
{
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return {.error=errno, .value={}};
    }
    assert(ws.ws_col != 0, "winsize.column<1");
    return {};
}


struct raw_moder {
    termios origin;
    raw_moder()
    {
        enable_raw_mode(&this->origin);
    }
    raw_moder(raw_moder&) = delete;
    raw_moder(raw_moder&&) = delete;
    raw_moder& operator=(raw_moder&) = delete;
    ~raw_moder()
    {
        disable_raw_mode(this->origin);
    }
};

int main()
{
    static raw_moder rm = {}; 

    for (;;) {
        char c = read_key();
        process_key(c);
    } 

    return 0;
}