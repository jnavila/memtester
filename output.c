// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Luca Ceresoli <luca@lucaceresoli.net>
 *
 * Output routines to conditionally show testing status.
 *
 * out_initialize() must be called at program startup and disabled status
 * output if stdout is not a tty. The other functions print test progress,
 * but only if on a tty.
 */

#include <stdio.h>
#include <unistd.h>

#include "output.h"

static int show_progress = 1;
static int wheel_pos;

void out_initialize()
{
    show_progress = isatty(STDOUT_FILENO);
}

void out_test_start()
{
    if (show_progress) {
        printf("           ");
        fflush(stdout);
    }
}

void out_test_setting(unsigned int j)
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("setting %3u", j);
        fflush(stdout);
    }
}

void out_test_testing(unsigned int j)
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b");
        printf("testing %3u", j);
        fflush(stdout);
    }
}

void out_test_end()
{
    if (show_progress) {
        printf("\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
        fflush(stdout);
    }
}

void out_wheel_start()
{
    if (show_progress) {
        putchar(' ');
        fflush(stdout);
        wheel_pos = 0;
    }
}

void out_wheel_advance(unsigned int i)
{
    static const unsigned int wheel_often = 2500;
    static const unsigned int n_chars = 4;
    char wheel_char[4] = {'-', '\\', '|', '/'};

    if (show_progress) {
        if (!(i % wheel_often)) {
            putchar('\b');
            putchar(wheel_char[++wheel_pos % n_chars]);
            fflush(stdout);
        }
    }
}

void out_wheel_end()
{
    if (show_progress) {
        printf("\b \b");
        fflush(stdout);
    }
}
