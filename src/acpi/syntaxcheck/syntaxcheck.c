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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "fwts.h"

static char *syntaxcheck_headline(void)
{
	return "Re-assemble DSDT and find syntax errors and warnings.";
}

static fwts_list* error_output;

static int syntaxcheck_init(fwts_framework *fw)
{
	if (fwts_check_root_euid(fw))
		return FWTS_ERROR;

	if (fwts_check_executable(fw, fw->iasl, "iasl"))
		return FWTS_ERROR;

	if (fwts_check_executable(fw, fw->acpidump, "acpidump"))
		return FWTS_ERROR;

	return FWTS_OK;
}

static int syntaxcheck_table(fwts_framework *fw, char *table, int which)
{
	fwts_list_element *item;
	int errors = 0;
	int warnings = 0;
	uint8 *tabledata;
	int size;

	tabledata = fwts_acpi_table_load(fw, table, which, &size);
	if (size == 0)
		return 2;		/* Table does not exist */

	if (tabledata == NULL) {
		fwts_log_error(fw, "Failed to load table for some reason!");
		return FWTS_ERROR;
	}

	error_output = fwts_iasl_reassemble(fw, tabledata, size);
	free(tabledata);
	if (error_output == NULL) {
		fwts_log_error(fw, "Cannot re-assasemble with iasl.");
		return FWTS_ERROR;
	}

	for (item = error_output->head; item != NULL; item = item->next) {
		int num;
		char ch;
		char *line = fwts_text_list_text(item);

		if ((sscanf(line, "%*s %d%c", &num, &ch) == 2) && ch == ':') {
			if (item->next != NULL) {
				char *nextline = fwts_text_list_text(item->next);
				if (!strncmp(nextline, "Error", 5)) {
					fwts_log_info_verbatum(fw, "%s", line);
					fwts_log_info_verbatum(fw, "%s", nextline);
					errors++;
				}
				if (!strncmp(nextline, "Warning", 7)) {
					fwts_log_info_verbatum(fw, "%s", line);
					fwts_log_info_verbatum(fw, "%s", nextline);
					warnings++;
				}
				item = item->next;
			}
			else {
				fwts_log_info(fw, "%s", line);
				fwts_log_error(fw, 
					"Could not find parser error message "
					"(this can happen if iasl segfaults!)");
			}
		}
	}
	fwts_text_list_free(error_output);

	if (errors > 0) {
		fwts_failed_high(fw, "Table %s (%d) reassembly: Found %d errors, %d warnings.", table, which, errors, warnings);
	} else if (warnings > 0) {
		fwts_failed_low(fw, "Table %s (%d) reassembly: Found 0 errors, %d warnings.", table, which, warnings);
	} else 
		fwts_passed(fw, "%s (%d) reassembly, Found 0 errors, 0 warnings.", table, which);

	return FWTS_OK;
}

static int syntaxcheck_DSDT(fwts_framework *fw)
{
	return syntaxcheck_table(fw, "DSDT", 0);
}

static int syntaxcheck_SSDT(fwts_framework *fw)
{
	int i;

	for (i=0; i < 100; i++) {
		int ret = syntaxcheck_table(fw, "SSDT", i);
		if (ret == 2)
			return FWTS_OK;	/* Hit the last table */
		if (ret != FWTS_OK)
			return FWTS_ERROR;
	}

	return FWTS_OK;
}

static fwts_framework_tests syntaxcheck_tests[] = {
	syntaxcheck_DSDT,
	syntaxcheck_SSDT,
	NULL
};

static fwts_framework_ops syntaxcheck_ops = {
	syntaxcheck_headline,
	syntaxcheck_init,	
	NULL,
	syntaxcheck_tests
};

FRAMEWORK(syntaxcheck, &syntaxcheck_ops, TEST_ANYTIME);
