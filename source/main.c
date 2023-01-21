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
#include <fcntl.h>
#include <linux/fb.h>
#include <term.h>
#include <backends/framebuffer.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <font.h>

char *const start_path = "/sbin/epoch";
char *const args[] = {start_path, "--init", NULL};

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
    struct term_context *term = fbterm_init(
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
    term->full_refresh(term);

    // Open the ps2 keyboard for use as new stdin.
    int kb = open("/dev/ps2keyboard", O_RDONLY);
    if (kb == -1) {
        perror("Could not open keyboard");
        return 1;
    }

    // Create a pipe for stdout/stderr.
    int master_pty;
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
    int input_child = fork();
    if (input_child == 0) {
        for (;;) {
            char input;
            read(kb, &input, 1);
            write(master_pty, &input, 1);
        }
    }

    // Catch what the child says.
    for (;;) {
        char output[512];
        ssize_t count = read(master_pty, &output, 512);
        for (ssize_t i = 0; i < count; i++) {
            if (output[i] == '\b') {
                term_write(term, "\b \b", 3);
            } else {
                term_write(term, &output[i], 1);
            }
        }
    }
}
