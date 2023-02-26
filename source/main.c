/*
    main.c: Entrypoint of the project
    Copyright (C) 2023 streaksu

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ttydefaults.h>
#include <term.h>
#include <backends/framebuffer.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <font.h>
#include <ctype.h>
#include <stdnoreturn.h>

static char *const start_path = "/sbin/epoch";
static char *const args[] = {start_path, "--init", NULL};

static int  kb;
static bool tty_mutex;
struct term_context *term;
static int master_pty;

static const char convtab_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const char convtab_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char convtab_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char convtab_nomod[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

#define SCANCODE_MAX 0x57
#define SCANCODE_CTRL 0x1d
#define SCANCODE_CTRL_REL 0x9d
#define SCANCODE_SHIFT_RIGHT 0x36
#define SCANCODE_SHIFT_RIGHT_REL 0xb6
#define SCANCODE_SHIFT_LEFT 0x2a
#define SCANCODE_SHIFT_LEFT_REL 0xaa
#define SCANCODE_ALT_LEFT 0x38
#define SCANCODE_ALT_LEFT_REL 0xb8
#define SCANCODE_CAPSLOCK 0x3a
#define SCANCODE_NUMLOCK 0x45

#define KBD_BUFFER_SIZE 1024
static char kbd_buffer[KBD_BUFFER_SIZE];
static size_t kbd_buffer_i = 0;

static bool decckm = false;

static void locked_term_write(const char *msg, size_t len) {
    while (__atomic_test_and_set(&tty_mutex, __ATOMIC_SEQ_CST)) {
        sched_yield();
    }
    term_write(term, msg, len);
    __atomic_clear(&tty_mutex, __ATOMIC_SEQ_CST);
}

static void dec_private(uint64_t esc_val_count, uint32_t *esc_values, uint64_t final) {
    (void)esc_val_count;

    switch (esc_values[0]) {
        case 1:
            switch (final) {
                case 'h': decckm = true;  break;
                case 'l': decckm = false; break;
            }
    }
}

static void limine_term_callback(struct term_context *t1, uint64_t t, uint64_t a, uint64_t b, uint64_t c) {
    (void)t1;

    switch (t) {
        case 10:
            dec_private(a, (void *)b, c);
    }
}

static void add_to_buf_char(struct termios *termios, char c, bool echo) {
    if (c == '\n' && (termios->c_iflag & ICRNL) == 0) {
        c = '\r';
    }

    if (termios->c_lflag & ICANON) {
        switch (c) {
            case '\n': {
                if (kbd_buffer_i == KBD_BUFFER_SIZE) {
                    return;
                }
                kbd_buffer[kbd_buffer_i++] = c;
                if (echo && (termios->c_lflag & ECHO)) {
                    locked_term_write("\n", 1);
                }
                write(master_pty, kbd_buffer, kbd_buffer_i);
                kbd_buffer_i = 0;
                return;
            }
            case '\b': {
                if (kbd_buffer_i == 0) {
                    return;
                }
                kbd_buffer_i--;
                size_t to_backspace;
                if (kbd_buffer[kbd_buffer_i] >= 0x01 && kbd_buffer[kbd_buffer_i] <= 0x1a) {
                    to_backspace = 2;
                } else {
                    to_backspace = 1;
                }
                kbd_buffer[kbd_buffer_i] = 0;
                if (echo && (termios->c_lflag & ECHO) != 0) {
                    for (size_t i = 0; i < to_backspace; i++) {
                        locked_term_write("\b \b", 3);
                    }
                }
                return;
            }
        }

        if (kbd_buffer_i == KBD_BUFFER_SIZE) {
            return;
        }
        kbd_buffer[kbd_buffer_i++] = c;
    } else {
        write(master_pty, &c, 1);
    }

    if (echo && (termios->c_lflag & ECHO) != 0) {
        if (c >= 0x20 && c <= 0x7e) {
            locked_term_write(&c, 1);
        } else if (c >= 0x01 && c <= 0x1a) {
            char caret[2];
            caret[0] = '^';
            caret[1] = c + 0x40;
            locked_term_write(caret, 2);
        }
    }
}

static void add_to_buf(struct termios *termios, char *ptr, size_t count, bool echo) {
    for (size_t i = 0; i < count; i++) {
        add_to_buf_char(termios, ptr[i], echo);
    }
}

static noreturn void *kb_input_thread(void *arg) {
    (void)arg;

    struct termios config;
    bool extra_scancodes = false;
    bool ctrl_active = false;
    //bool numlock_active = false;
    //bool alt_active = false;
    bool shift_active = false;
    bool capslock_active = false;

    for (;;) {
        uint8_t input_byte;
        read(kb, &input_byte, 1);
        if (tcgetattr(master_pty, &config) < 0) {
            perror("Could not fetch termios in keyboard input thread");
        }

        if (input_byte == 0xe0) {
            extra_scancodes = true;
            continue;
        }

        if (extra_scancodes == true) {
            extra_scancodes = false;

            switch (input_byte) {
                case SCANCODE_CTRL:
                    ctrl_active = true;
                    continue;
                case SCANCODE_CTRL_REL:
                    ctrl_active = false;
                    continue;
                case 0x1c:
                    add_to_buf(&config, "\n", 1, true);
                    continue;
                case 0x35:
                    add_to_buf(&config, "/", 1, true);
                    continue;
                case 0x48: // up arrow
                    if (decckm == false) {
                        add_to_buf(&config, "\e[A", 3, true);
                    } else {
                        add_to_buf(&config, "\eOA", 3, true);
                    }
                    continue;
                case 0x4b: // left arrow
                    if (decckm == false) {
                        add_to_buf(&config, "\e[D", 3, true);
                    } else {
                        add_to_buf(&config, "\eOD", 3, true);
                    }
                    continue;
                case 0x50: // down arrow
                    if (decckm == false) {
                        add_to_buf(&config, "\e[B", 3, true);
                    } else {
                        add_to_buf(&config, "\eOB", 3, true);
                    }
                    continue;
                case 0x4d: // right arrow
                    if (decckm == false) {
                        add_to_buf(&config, "\e[C", 3, true);
                    } else {
                        add_to_buf(&config, "\eOC", 3, true);
                    }
                    continue;
                case 0x47: // home
                    add_to_buf(&config, "\e[1~", 4, true);
                    continue;
                case 0x4f: // end
                    add_to_buf(&config, "\e[4~", 4, true);
                    continue;
                case 0x49: // pgup
                    add_to_buf(&config, "\e[5~", 4, true);
                    continue;
                case 0x51: // pgdown
                    add_to_buf(&config, "\e[6~", 4, true);
                    continue;
                case 0x53: // delete
                    add_to_buf(&config, "\e[3~", 4, true);
                    continue;
            }
        }

        switch (input_byte) {
            case SCANCODE_NUMLOCK:
                //numlock_active = true;
                continue;
            case SCANCODE_ALT_LEFT:
                //alt_active = true;
                continue;
            case SCANCODE_ALT_LEFT_REL:
                //alt_active = false;
                continue;
            case SCANCODE_SHIFT_LEFT:
            case SCANCODE_SHIFT_RIGHT:
                shift_active = true;
                continue;
            case SCANCODE_SHIFT_LEFT_REL:
            case SCANCODE_SHIFT_RIGHT_REL:
                shift_active = false;
                continue;
            case SCANCODE_CTRL:
                ctrl_active = true;
                continue;
            case SCANCODE_CTRL_REL:
                ctrl_active = false;
                continue;
            case SCANCODE_CAPSLOCK:
                capslock_active = !capslock_active;
                continue;
        }

        char c = 0;

        if (input_byte < SCANCODE_MAX) {
            if (capslock_active == false && shift_active == false) {
                c = convtab_nomod[input_byte];
            }
            if (capslock_active == false && shift_active == true) {
                c = convtab_shift[input_byte];
            }
            if (capslock_active == true && shift_active == false) {
                c = convtab_capslock[input_byte];
            }
            if (capslock_active == true && shift_active == true) {
                c = convtab_shift_capslock[input_byte];
            }
        } else {
            continue;
        }

        if (ctrl_active) {
            c = toupper(c) - 0x40;
        }

        add_to_buf(&config, &c, 1, true);
    }
}

int main(void) {
    // Initialize the tty.
    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;
    int fb = open("/dev/fb0", O_RDWR);
    if (fb == -1) {
        perror("Could not open framebuffer");
        return 1;
    }

    if (ioctl(fb, FBIOGET_VSCREENINFO, &var_info) == -1) {
        perror("Could not fetch framebuffer properties");
        return 1;
    }
    if (ioctl(fb, FBIOGET_FSCREENINFO, &fix_info) == -1) {
        perror("Could not fetch framebuffer properties");
        return 1;
    }

    size_t pixel_size  = fix_info.smem_len / sizeof(uint32_t);
    size_t linear_size = pixel_size * sizeof(uint32_t);
    size_t aligned_size = (linear_size + 0x1000 - 1) & ~(0x1000 - 1);
    uint32_t *mem_window = mmap(
        NULL,
        aligned_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fb,
        0
    );
    if (mem_window == NULL) {
        perror("Could not mmap framebuffer");
        return 1;
    }

    // Initialize the terminal.
    uint32_t background = 0x1E1E1E;
    uint32_t foreground = 0xD8D8D8;
    uint32_t dark_palette[] = {
        0x241F31,
        0xC01C28,
        0x2EC27E,
        0xF5C211,
        0x1E78E4,
        0x9841BB,
        0x0AB9D0,
        0xC0BFBC
    };
    uint32_t bright_palette[] = {
        0x5E5C64,
        0xED333B,
        0x57E389,
        0xF8E45C,
        0x51A1FF,
        0xC061CB,
        0x4FD2FD,
        0xF6F5F4
    };
    term = fbterm_init(
        malloc,
        mem_window,
        var_info.xres,
        var_info.yres,
        fix_info.smem_len / var_info.yres,
        NULL,
        dark_palette,
        bright_palette,
        &background,
        &foreground,
        &background,
        &foreground,
        unifont_arr,
        FONT_WIDTH,
        FONT_HEIGHT,
        0,
        1,
        1,
        0
    );
    term->callback = limine_term_callback;
    term->full_refresh(term);

    // Free the mutex.
    __atomic_clear(&tty_mutex, __ATOMIC_SEQ_CST);

    // Open the ps2 keyboard for use as new stdin.
    kb = open("/dev/ps2keyboard", O_RDONLY);
    if (kb == -1) {
        perror("Could not open keyboard");
        return 1;
    }

    // Create a pipe for stdout/stderr.
    int pty_spawner = open("/dev/ptmx", O_RDWR);
    if (pty_spawner == -1) {
      perror("Could not open ptmx");
      return 1;
    }

    if (ioctl(pty_spawner, 0, &master_pty) != 0) {
        perror("Could not create pty");
        return 1;
    }

    struct winsize win_size = {
        .ws_row = var_info.yres / FONT_HEIGHT,
        .ws_col = var_info.xres / FONT_WIDTH,
        .ws_xpixel = var_info.xres,
        .ws_ypixel = var_info.yres
    };
    if (ioctl(master_pty, TIOCSWINSZ, &win_size) == -1) {
        perror("Could not set pty size");
        return 1;
    }

    struct termios termios;
    if (tcgetattr(master_pty, &termios) < 0) {
        perror("Could not fetch termios");
    }

    termios.c_iflag = BRKINT | IGNPAR | ICRNL | IXON | IMAXBEL;
    termios.c_oflag = OPOST | ONLCR;
    termios.c_cflag = CS8 | CREAD;
    termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE;
    termios.c_cc[VINTR] = CTRL('C');
    termios.c_cc[VEOF] = CTRL('D');
    termios.c_cc[VSUSP] = CTRL('Z');
    termios.ibaud = 38400;
    termios.obaud = 38400;

    if (tcsetattr(master_pty, TCSAFLUSH, &termios) < 0) {
        perror("Could not set termios");
    }

    // Export some variables related to the TTY.
    putenv("TERM=linux");

    // Boot the child.
    int child = fork();
    if (child == 0) {
        // Replace std streams.
        int slave_pty;
        if (ioctl(master_pty, 0, &slave_pty) != 0) {
            perror("Could not create pty");
            return 1;
        }

        dup2(slave_pty, 0);
        dup2(slave_pty, 1);
        dup2(slave_pty, 2);
        execvp(start_path, args);
        perror("Could not start");
        return 1;
    }

    // Boot an input process.
    pthread_t input_thread;
    if (pthread_create(&input_thread, NULL, kb_input_thread, NULL)) {
        perror("Could not create input thread!");
    }

    // Catch what the child says.
    for (;;) {
        char output[512];
        ssize_t count = read(master_pty, output, 512);
        if (count != 0) {
            locked_term_write(output, count);
        }
    }
}
