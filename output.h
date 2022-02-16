// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Luca Ceresoli <luca@lucaceresoli.net>
 *
 * Output routines to conditionally show testing status.
 */

#ifndef _OUTPUT_H_
#define _OUTPUT_H_

void out_initialize();

void out_test_start();
void out_test_setting();
void out_test_testing();
void out_test_end();

void out_wheel_start();
void out_wheel_advance();
void out_wheel_end();

#endif // _OUTPUT_H_
