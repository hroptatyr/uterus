/*** ute.c -- command line tool for libuterus
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of sushi/uterus.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

/** test client for libuterus */
#include <stddef.h>
#include <stdio.h>
#include "utefile.h"
#include "sl1t.h"

/* simple all in one tool */

#include "ute-print.c"
#include "ute-mux.c"
#include "ute-shnot.h"

static void
ute_cmd_sort(const char *file)
{
	void *hdl = ute_open(file, UO_RDWR);
	ute_sort(hdl);
	ute_close(hdl);
	return;
}

static void
ute_test(const char *file)
{
	void *hdl = ute_open(file, UO_CREAT | UO_TRUNC);
	struct sl1t_s t[1];
	for (int i = 0; i < 4194304; i++) {
		t[0].hdr->sec = 1234567890 - i + ((i % 900) ? 0 : 900);
		t[0].hdr->msec = (uint16_t)(i % 77);
		t[0].hdr->ttf = SL1T_TTF_TRA;
		t[0].v[0] = 1234 + i;
		/* add him */
		ute_add_tick(hdl, AS_SCOM(t));
	}
	ute_close(hdl);
	return;
}

static void
ute_cmd_tes2(const char *file)
{
	void *hdl = ute_open(file, UO_CREAT | UO_TRUNC);
	struct sl1t_s t[1];
	for (int i = 0; i < 80000000; i++) {
		t[0].hdr->sec = 1234567890 + i + ((i % 900) ? 0 : -900);
		t[0].hdr->msec = (uint16_t)(i % 77);
		t[0].hdr->ttf = SL1T_TTF_TRA;
		t[0].v[0] = 1234 + i;
		/* add him */
		ute_add_tick(hdl, AS_SCOM(t));
	}
	ute_close(hdl);
	return;
}

static void
ute_cmd_tes3(const char *file)
{
	void *hdl = ute_open(file, UO_CREAT | UO_TRUNC);
	struct sl1t_s t[1];
	for (int i = 0; i < 6000000; i++) {
		t[0].hdr->sec = 1234567890 + (i % 114000) + ((i * i) % 17);
		t[0].hdr->msec = (uint16_t)(i % 77);
		t[0].hdr->ttf = SL1T_TTF_TRA;
		t[0].v[0] = 1234 + i;
		/* add him */
		ute_add_tick(hdl, AS_SCOM(t));
	}
	ute_close(hdl);
	return;
}

static void
ute_cmd_tes4(const char *file)
{
	void *hdl = ute_open(file, UO_CREAT | UO_TRUNC);
	struct sl1t_s t[1];
	for (int i = 0; i < 80000000; i++) {
		t[0].hdr->sec = 1234567890 + i + (i % 262144 == 10 ? -4 : 0);
		t[0].hdr->msec = (uint16_t)(i % 77);
		t[0].hdr->ttf = SL1T_TTF_TRA;
		t[0].v[0] = 1234 + i;
		/* add him */
		ute_add_tick(hdl, AS_SCOM(t));
	}
	ute_close(hdl);
	return;
}

static void
ute_help(const char *cmd)
{
	if (cmd == NULL || strcmp(cmd, "help") == 0) {
		fputs("usage: ute COMMAND ARGS\n"
		      "\n"
		      "Supported commands:\n"
		      "  help    This help screen\n"
		      "  mux     Generate an ute file from a tick source\n"
		      "  print   Print the contents of an ute file\n"
		      "  shnot   Generate top-level snapshots from ute files\n"
		      "  sort    Test sort the file\n"
		      "\n"
		      "Use 'ute help COMMAND' for more information "
		      "on a specific command\n", stdout);

	} else if (strcmp(cmd, "mux") == 0) {
		fputs(ute_cmd_mux_help, stdout);

	} else if (strcmp(cmd, "print") == 0) {
		fputs(ute_cmd_print_help, stdout);
	}
	return;
}

int
main(int argc, const char *argv[])
{
	if (argc <= 1) {
		goto out;

	} else if (strcmp(argv[1], "mux") == 0) {
		ute_cmd_mux_args(argc - 1, argv + 1);

	} else if (strcmp(argv[1], "print") == 0) {
		ute_cmd_print_args(argc - 1, argv + 1);

	} else if (strcmp(argv[1], "shnot") == 0) {
		return ute_cmd_shnot_args(argc - 1, argv + 1);

	} else if (strcmp(argv[1], "sort") == 0) {
		if (argv[2] == NULL) {
			goto out;
		}
		ute_cmd_sort(argv[2]);

	} else if (strcmp(argv[1], "test") == 0) {
		if (argv[2] == NULL) {
			goto out;
		}
		ute_test(argv[2]);

	} else if (strcmp(argv[1], "tes2") == 0) {
		if (argv[2] == NULL) {
			goto out;
		}
		ute_cmd_tes2(argv[2]);

	} else if (strcmp(argv[1], "tes3") == 0) {
		if (argv[2] == NULL) {
			goto out;
		}
		ute_cmd_tes3(argv[2]);

	} else if (strcmp(argv[1], "tes4") == 0) {
		if (argv[2] == NULL) {
			goto out;
		}
		ute_cmd_tes4(argv[2]);

	} else {
	out:
		ute_help(argv[2]);
		return 1;
	}
	return 0;
}

/* ute.c ends here */
