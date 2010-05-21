/*
 * Copyright (C) 2010 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "log.h"

static char *log_prefix_to_str(log_prefix prefix)
{
	switch (prefix) {
	case LOG_RESULT:
		return "RES";
	case LOG_ERROR:
		return "ERR";
	case LOG_WARNING:
		return "WRN";
	case LOG_DEBUG:
		return "DBG";
	case LOG_INFO:
		return "INF";
	case LOG_SUMMARY:
		return "SUM";
	default:
		return "???";
	}
}

int log_printf(log *log, log_prefix prefix, const char *fmt, ...)
{
	char buffer[1024];
	int n = 0;
	struct tm tm;
	time_t now;

	if ((!log) || (log && log->magic != LOG_MAGIC))
		return 0;

	va_list ap;

	va_start(ap, fmt);

	time(&now);
	localtime_r(&now, &tm);

	n = snprintf(buffer, sizeof(buffer), 
		"%2.2d/%2.2d/%-2.2d %2.2d:%2.2d:%2.2d %s ", 		
		tm.tm_mday, tm.tm_mon, (tm.tm_year+1900) % 100,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		log_prefix_to_str(prefix));

	if (log->owner)
		n += snprintf(buffer+n, sizeof(buffer)-n, "(%s): ", log->owner);

	vsnprintf(buffer+n, sizeof(buffer)-n, fmt, ap);

	n = fwrite(buffer, 1, strlen(buffer), log->fp);
	fflush(log->fp);

	va_end(ap);

	return n;
}

void log_underline(log *log, int ch)
{
	int i;

	if ((!log) || (log && log->magic != LOG_MAGIC))
		return;

	for (i=0;i<80;i++) {
		fputc(ch, log->fp);
	}
	fputc('\n', log->fp);
	fflush(log->fp);
}

void log_newline(log *log)
{
	if ((!log) || (log && log->magic != LOG_MAGIC))
		return;

	fprintf(log->fp, "\n");
	fflush(log->fp);
}

log *log_open(const char *owner, const char *name, const char *mode)
{
	log *newlog;

	if ((newlog = malloc(sizeof(log))) == NULL) {
		return NULL;
	}

	newlog->magic = LOG_MAGIC;

	if (owner) {
		if ((newlog->owner = malloc(strlen(owner)+1)) == NULL) {		
			free(newlog);
			return NULL;
		}
		strcpy(newlog->owner, owner);
	}

	if (strcmp("stderr", name) == 0) {
		newlog->fp = stderr;
	} else if (strcmp("stdout", name) == 0) {
		newlog->fp = stdout;
	} else if ((newlog->fp = fopen(name, mode)) == NULL) {
		free(newlog);
		return NULL;
	}

	return newlog;
}

int log_close(log *log)
{
	if (log && (log->magic == LOG_MAGIC)) {
		if (log->fp && (log->fp != stdout && log->fp != stderr))
			fclose(log->fp);
		if (log->owner)
			free(log->owner);
		free(log);
	}
	return 0;
}
