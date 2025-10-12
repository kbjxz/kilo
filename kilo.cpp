#include "lib/lib.h"
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <array>
#include <sys/ioctl.h>

static const auto screen_clear = string_from("\x1b[2J");
static const auto cursor_reposition = string_from("\x1b[H");
static const auto cursor_hide = string_from("\x1b[?25l");
static const auto cursor_show = string_from("\x1b[?25h");
static const auto tilde = string_from("~\r\n");
static const auto last_tilde = string_from("~");
static const auto line_erase_all = string_from("\x1b[2K");
static const auto line_erase_left = string_from("\x1b[1K");
static const auto line_erase_right = string_from("\x1b[K");

void clear_screen()
{
    write(STDOUT_FILENO, screen_clear.data, screen_clear.len);
    write(STDOUT_FILENO, cursor_reposition.data, cursor_reposition.len);
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
        return {
            .error={.err_no=errno, .msg="ioctl"}, 
            .value={}};
    }
    assert(ws.ws_col != 0, "winsize.column<1");
    return {};
}


void draw_rows(string* buf, const winsize wz, arena* a)
{
    for (auto i = 0; i < wz.ws_row-1; i++) {
        string_append(buf, &line_erase_right, a);
        string_append(buf, &tilde, a);
    }
    string_append(buf, &last_tilde, a);
}

void refresh_screen(const winsize wz, arena* a)
{
    arena scratch = {};
    arena_scratch_from(&scratch, a);
    a = &scratch;

    const auto len = 
        (cursor_hide.len)
        + (screen_clear.len)
        + (cursor_reposition.len)
        + (wz.ws_row-1) * (line_erase_right.len + tilde.len) + last_tilde.len 
        + (cursor_reposition.len)
        + (cursor_show.len);
    string buf = {};
    string_reserve(&buf, len, a);

    string_append(&buf, cursor_hide.data, cursor_hide.len, a);
    string_append(&buf, screen_clear.data, screen_clear.len, a);    
    string_append(&buf, cursor_reposition.data, cursor_reposition.len, a);    
    draw_rows(&buf, wz, a);
    string_append(&buf, cursor_reposition.data, cursor_reposition.len, a);    
    string_append(&buf, cursor_show.data, cursor_show.len, a);    
    
    write(STDOUT_FILENO, buf.data, buf.len);
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