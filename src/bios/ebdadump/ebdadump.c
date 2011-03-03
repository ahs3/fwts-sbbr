/*
 * Copyright (C) 2011 Canonical
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

#include "fwts.h"

#ifdef FWTS_ARCH_INTEL

#define EBDA_OFFSET		0x40e
#define BIOS_ROM_START		(0x000a0000)

static int ebdadump_init(fwts_framework *fw)
{
	if (fwts_check_root_euid(fw))
		return FWTS_ERROR;

	return FWTS_OK;
}

static char *ebdadump_headline(void)
{
	return "Dump EBDA region.";
}

static void ebdadump_data(fwts_framework *fw, uint8_t *data, int offset, int length)
{
	char buffer[128];
	int i;

	for (i=0; i<length; i+=16) {
		fwts_dump_raw_data(buffer, sizeof(buffer), data+i, offset+i, 16);
		fwts_log_info_verbatum(fw, "%s", buffer);
	}
}

static int ebdadump_test1(fwts_framework *fw)
{
	uint8_t *mem;
	uint16_t *ebda;
	off_t  ebda_addr;
	size_t len;

	if ((ebda = fwts_mmap((off_t)EBDA_OFFSET, sizeof(uint16_t))) == FWTS_MAP_FAILED) {
		fwts_log_error(fw, "Failed to get EBDA.");
		return FWTS_ERROR;
	}
	ebda_addr = ((off_t)*ebda) << 4;
	(void)fwts_munmap(ebda, sizeof(uint16_t));

	len = BIOS_ROM_START - ebda_addr;

	if (ebda_addr > BIOS_ROM_START) {
		fwts_log_error(fw, "EBDA start address is greater than the BIOS ROM start address.");		
		return FWTS_ERROR;
	}

        if ((mem = fwts_mmap(ebda_addr, len)) == MAP_FAILED) {
		fwts_log_error(fw, "Cannot mmap BIOS ROM region.");
		return FWTS_ERROR;
	}

	fwts_log_info(fw, "EBDA region: %x..%x (%d bytes)", (unsigned int)ebda_addr, 
		BIOS_ROM_START, (unsigned int)len);
	
	ebdadump_data(fw, mem, ebda_addr, len);
        (void)fwts_munmap(mem, len);

	fwts_infoonly(fw);

	return FWTS_OK;
}

static fwts_framework_minor_test ebdadump_tests[] = {
	{ ebdadump_test1, "Dump EBDA region." },
	{ NULL, NULL }
};

static fwts_framework_ops ebdadump_ops = {
	.headline    = ebdadump_headline,
	.init        = ebdadump_init,
	.minor_tests = ebdadump_tests
};

FWTS_REGISTER(ebdadump, &ebdadump_ops, FWTS_TEST_ANYTIME, FWTS_UTILS);

#endif