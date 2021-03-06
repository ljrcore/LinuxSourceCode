/*
 * Copyright (C) 1996 Paul Mackerras.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sysrq.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/udbg.h>

extern void xmon_printf(const char *fmt, ...);

#define TB_SPEED	25000000

static inline unsigned int readtb(void)
{
	unsigned int ret;

	asm volatile("mftb %0" : "=r" (ret) :);
	return ret;
}

static void sysrq_handle_xmon(int key, struct pt_regs *pt_regs,
			      struct tty_struct *tty) 
{
	xmon(pt_regs);
}
static struct sysrq_key_op sysrq_xmon_op = 
{
	.handler =	sysrq_handle_xmon,
	.help_msg =	"xmon",
	.action_msg =	"Entering xmon\n",
};

void
xmon_map_scc(void)
{
	/* This maybe isn't the best place to register sysrq 'x' */
	__sysrq_put_key_op('x', &sysrq_xmon_op);
}

int
xmon_write(void *handle, void *ptr, int nb)
{
	return udbg_write(ptr, nb);
}

int xmon_wants_key;

int
xmon_read(void *handle, void *ptr, int nb)
{
	return udbg_read(ptr, nb);
}

int
xmon_read_poll(void)
{
	return udbg_getc_poll();
}
 
void *xmon_stdin;
void *xmon_stdout;
void *xmon_stderr;

void
xmon_init(void)
{
}

int
xmon_putc(int c, void *f)
{
	char ch = c;

	if (c == '\n')
		xmon_putc('\r', f);
	return xmon_write(f, &ch, 1) == 1? c: -1;
}

int
xmon_putchar(int c)
{
	return xmon_putc(c, xmon_stdout);
}

int
xmon_fputs(char *str, void *f)
{
	int n = strlen(str);

	return xmon_write(f, str, n) == n? 0: -1;
}

int
xmon_readchar(void)
{
	char ch;

	for (;;) {
		switch (xmon_read(xmon_stdin, &ch, 1)) {
		case 1:
			return ch;
		case -1:
			xmon_printf("read(stdin) returned -1\r\n", 0, 0);
			return -1;
		}
	}
}

static char line[256];
static char *lineptr;
static int lineleft;

int
xmon_getchar(void)
{
	int c;

	if (lineleft == 0) {
		lineptr = line;
		for (;;) {
			c = xmon_readchar();
			if (c == -1 || c == 4)
				break;
			if (c == '\r' || c == '\n') {
				*lineptr++ = '\n';
				xmon_putchar('\n');
				break;
			}
			switch (c) {
			case 0177:
			case '\b':
				if (lineptr > line) {
					xmon_putchar('\b');
					xmon_putchar(' ');
					xmon_putchar('\b');
					--lineptr;
				}
				break;
			case 'U' & 0x1F:
				while (lineptr > line) {
					xmon_putchar('\b');
					xmon_putchar(' ');
					xmon_putchar('\b');
					--lineptr;
				}
				break;
			default:
				if (lineptr >= &line[sizeof(line) - 1])
					xmon_putchar('\a');
				else {
					xmon_putchar(c);
					*lineptr++ = c;
				}
			}
		}
		lineleft = lineptr - line;
		lineptr = line;
	}
	if (lineleft == 0)
		return -1;
	--lineleft;
	return *lineptr++;
}

char *
xmon_fgets(char *str, int nb, void *f)
{
	char *p;
	int c;

	for (p = str; p < str + nb - 1; ) {
		c = xmon_getchar();
		if (c == -1) {
			if (p == str)
				return 0;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = 0;
	return str;
}
