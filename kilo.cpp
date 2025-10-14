#include "lib/lib.h"
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define KILO_VERSION "0.1"
static const auto screen_clear = string_from("\x1b[2J");
static const auto cursor_reposition = string_from("\x1b[H");
static const auto cursor_hide = string_from("\x1b[?25l");
static const auto cursor_show = string_from("\x1b[?25h");
static const auto tilde_new_line = string_from("~\r\n");
static const auto line_erase_all = string_from("\x1b[2K");
static const auto line_erase_left = string_from("\x1b[1K");
static const auto line_erase_right = string_from("\x1b[K");
static const auto welcome_prefix= string_from("Kilo editor -- version ");
static const auto kilo_version = string_from(KILO_VERSION);
static const auto newline = string_from("\r\n");

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
    if (!isatty(STDIN_FILENO)) {
        die("STDIN_FILENO not associated with terminal");
    }

    if (tcgetattr(STDIN_FILENO, origin_termios) == -1) {
        die("tcgetattr");
    }
    
    termios raw_mode = *origin_termios;

    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    /* output modes - disable post processing */
    raw_mode.c_oflag &= ~(OPOST);
    
    /* control modes - set 8 bit chars */
    raw_mode.c_cflag |= (CS8);
    
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw_mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* control chars - set return condition: min number of bytes and timer. */
    raw_mode.c_cc[VMIN] = 0;  /* Return each byte, or zero for timeout. */
    raw_mode.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    /* put terminal in raw mode after flushing */
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
        return result_err<winsize>({.err_no=errno, .msg="ioctl"}); 
    }
    assert(ws.ws_col != 0, "winsize.column<1");
    return result_v<winsize>(ws); 
}

void append_tilde_erase_right(string* buf, bool linebreak, arena* a)
{
    string_append(buf, '~', a);
    string_append(buf, &line_erase_right, a);
    if (linebreak) {
        string_append(buf, &newline, a);
    }
}

void draw_rows(string* buf, const winsize wz, arena* a)
{
    for (auto i = 0; i < wz.ws_row; i++) {
        if (i != wz.ws_row / 3) {
            append_tilde_erase_right(buf, i < wz.ws_row - 1, a); 
            continue;
        }

        // draw welcome message in screen
        const auto welcome_len = 
            (welcome_prefix.len + kilo_version.len) <= wz.ws_col 
            ? (welcome_prefix.len + kilo_version.len) 
            : wz.ws_col;
        const auto padding_len = (wz.ws_col - welcome_len) / 2;
        if (padding_len > 0) {
            string_append(buf, '~', a);
        }
        for (size_t i = 0; i + 1 < padding_len; i++) {
            string_append(buf, ' ', a);
        }
        string_append(buf, &welcome_prefix, a);
        string_append(buf, &kilo_version, a);
    }
}

void refresh_screen(const winsize wz, arena* a)
{
    arena scratch = {};
    arena_scratch_from(&scratch, a);
    a = &scratch;

    string buf = {};
    string_reserve(&buf, 8192, a);

    string_append(&buf, &cursor_hide, a);
    string_append(&buf, &cursor_reposition, a);    
    draw_rows(&buf, wz, a);
    string_append(&buf, &cursor_reposition, a);    
    string_append(&buf, &cursor_show, a);    
    
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
    arena a = arena_new(4 * 1024 * 1024, ARENA_STRATEGY_PANIC);

    for (;;) {
        const auto winsize_result = get_window_size();
        if (result_is_err(&winsize_result)) {
            die(result_unwrap_err(&winsize_result).msg);
        }

        refresh_screen(winsize_result.value, &a);
        char c = read_key();
        process_key(c);
    } 

    return 0;
}