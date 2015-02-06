/*
 * Copyright (C) 2014-2015 Canonical
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

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "fwts.h"
#include "fwts_uefi.h"
#include "efi_runtime.h"
#include "fwts_efi_module.h"
#include "authvardefs.h"

static int fd;

#define TEST_GUID {0x7f5c5d52, 0x2f14, 0x4f12, {0x96, 0x7c, 0xdb, 0x60, 0xdb, 0x05, 0xa0, 0xfd} }
#define TEST_GUID1 {0x0ef2aa27, 0x1e93, 0x4284, {0xa1, 0xf9, 0x34, 0xd5, 0x6c, 0x5c, 0xde, 0x84} }

#define getvar_buf_size 100

static EFI_GUID gtestguid = TEST_GUID;

static const uint32_t attributes =
	FWTS_UEFI_VAR_NON_VOLATILE |
	FWTS_UEFI_VAR_BOOTSERVICE_ACCESS |
	FWTS_UEFI_VAR_RUNTIME_ACCESS |
	FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS;

static uint16_t variablenametest[] = {'A', 'u', 't', 'h', 'V', 'a', 'r', 'T', 'e', 's', 't', '\0'};

static long setvar(
	EFI_GUID *guid,
	uint32_t attributes,
	uint64_t datasize,
	void *data,
	uint64_t *status)
{
	long ioret;
	struct efi_setvariable setvariable;

	setvariable.VariableName = variablenametest;
	setvariable.VendorGuid = guid;
	setvariable.Attributes = attributes;
	setvariable.DataSize = datasize;
	setvariable.Data = data;
	setvariable.status = status;
	ioret = ioctl(fd, EFI_RUNTIME_SET_VARIABLE, &setvariable);

	return ioret;
}

static long getvar(
	EFI_GUID *guid,
	uint32_t *attributestest,
	uint64_t *getdatasize,
	void *data,
	uint64_t *status)
{
	long ioret;
	struct efi_getvariable getvariable;

	getvariable.VariableName = variablenametest;
	getvariable.VendorGuid = guid;
	getvariable.Attributes = attributestest;
	getvariable.DataSize = getdatasize;
	getvariable.Data = data;
	getvariable.status = status;
	ioret = ioctl(fd, EFI_RUNTIME_GET_VARIABLE, &getvariable);

	return ioret;
}

static void uefirtvariable_env_cleanup(void)
{
	uint64_t status;
	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint32_t attributestest;

	long ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret != -1 && status == EFI_SUCCESS) {
		setvar(&gtestguid, attributes, sizeof(AuthVarDel), AuthVarDel, &status);
		setvar(&gtestguid, attributes, sizeof(AuthVarDelDiff), AuthVarDelDiff, &status);
	}
	return;
}

static int uefirtauthvar_init(fwts_framework *fw)
{
	if (fwts_firmware_detect() != FWTS_FIRMWARE_UEFI) {
		fwts_log_info(fw, "Cannot detect any UEFI firmware. Aborted.");
		return FWTS_ABORTED;
	}

	if (fwts_lib_efi_runtime_load_module(fw) != FWTS_OK) {
		fwts_log_info(fw, "Cannot load efi_runtime module. Aborted.");
		return FWTS_ABORTED;
	}

	fd = open("/dev/efi_runtime", O_WRONLY | O_RDWR);
	if (fd == -1) {
		fwts_log_info(fw, "Cannot open efi_runtime driver. Aborted.");
		return FWTS_ABORTED;
	}

	uefirtvariable_env_cleanup();

	return FWTS_OK;
}

static int uefirtauthvar_deinit(fwts_framework *fw)
{
	close(fd);
	fwts_lib_efi_runtime_unload_module(fw);

	return FWTS_OK;
}

static int check_fw_support(fwts_framework *fw, uint64_t status)
{
	if ((status == EFI_INVALID_PARAMETER) &&
		((attributes & FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS) ||
		(attributes & FWTS_UEFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS) ||
		(attributes & FWTS_UEFI_VARIABLE_APPEND_WRITE))) {
		fwts_uefi_print_status_info(fw, status);
		fwts_skipped(fw,
			"EFI_INVALID_PARAMETER shall be returned, "
			"when firmware doesn't support these operations "
			"with EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS, "
			"EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS "
			"or EFI_VARIABLE_APPEND_WRITE attributes is set.");
		return FWTS_SKIP;
	}
	if (status == EFI_OUT_OF_RESOURCES) {
		fwts_uefi_print_status_info(fw, status);
		fwts_skipped(fw,
			"Run out of resources for SetVariable "
			"UEFI runtime interface: cannot test.");
		fwts_advice(fw,
			"Firmware may reclaim some resources "
			"after rebooting. Reboot and test "
			"again may be helpful to continue "
			"the test.");
		return FWTS_SKIP;
	}
	return FWTS_OK;
}

/*
 * Set the created authenticated variable, AuthVarCreate,
 * and checking the data size and data.
 * expect EFI_SUCCESS returned.
 */
static int uefirtauthvar_test1(fwts_framework *fw)
{
	long ioret;

	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint64_t status;
	uint32_t attributestest;
	size_t i;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFICreateAuthVar",
			"Failed to create authenticated variable with UEFI "
			"runtime service.");

		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFICreateAuthVar",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	if (getdatasize != sizeof(AuthVarCreateData)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFICreateAuthVar",
			"Get authenticated variable data size is not the "
			"same as it set.");
	}
	for (i = 0; i < sizeof(AuthVarCreateData); i++) {
		if (data[i] != AuthVarCreateData[i]) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFICreateAuthVar",
			"Get authenticated variable data are not the "
			"same as it set.");
			return FWTS_ERROR;
		}
	}

	fwts_passed(fw, "Create authenticated variable test passed.");

	return FWTS_OK;
}

/*
 * With one existing variable, but set the same authenticated variable,
 * AuthVarCreate, expect EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test2(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Set the same authenticated variable test passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetSameAuthVar",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetSameAuthVar",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * With one existing variable, but set authenticated variable which created by another valid key,
 * expect EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test3(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarCreateDiff), AuthVarCreateDiff, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Authenticated variable test with another valid authenticated variable passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetDiffAuthVar",
			"Set different valid authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetDiffAuthVar",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * Execute the normal append operation.
 */
static int uefirtauthvar_test4(fwts_framework *fw)
{
	long ioret;

	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint64_t status;
	uint32_t attributestest;
	size_t i;
	uint32_t attribappend = attributes | FWTS_UEFI_VARIABLE_APPEND_WRITE;

	ioret = setvar(&gtestguid, attribappend, sizeof(AuthVarAppend), AuthVarAppend, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIAppendAuthVar",
			"Failed to append authenticated variable with UEFI "
			"runtime service.");

		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIAppendAuthVar",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (getdatasize != (sizeof(AuthVarCreateData) + sizeof(AuthVarAppendData))) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIAppendAuthVar",
			"Get total authenticated variable data size is not the "
			"same as it set and appended.");
	}

	for (i = 0; i < getdatasize; i++) {
		if (i < sizeof(AuthVarCreateData)) {
			if (data[i] != AuthVarCreateData[i]) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIAppendAuthVar",
				"Get authenticated variable data are not the "
				"same as it set.");
				return FWTS_ERROR;
			}
		} else {
			if (data[i] != AuthVarAppendData[i - sizeof(AuthVarCreateData)]) {
				fwts_failed(fw, LOG_LEVEL_HIGH,
				"UEFIAppendAuthVar",
				"Get authenticated variable data are not the "
				"same as it set.");
				return FWTS_ERROR;
			}
		}
	}

	fwts_passed(fw, "Append authenticated variable tests passed.");

	return FWTS_OK;
}

/*
 * Update the new authenticated variable by using the same key but a new
 * timestamp and data.
 */
static int uefirtauthvar_test5(fwts_framework *fw)
{
	long ioret;

	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint64_t status;
	uint32_t attributestest;
	size_t i;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarUpdate), AuthVarUpdate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIUpdateAuthVar",
			"Failed to update authenticated variable with UEFI "
			"runtime service.");

		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIUpdateAuthVar",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	if (getdatasize != sizeof(AuthVarUpdateData)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIUpdateAuthVar",
			"Get authenticated variable data size is not the "
			"same as it set.");
	}

	for (i = 0; i < getdatasize; i++) {
		if (data[i] != AuthVarUpdateData[i]) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIUpdateAuthVar",
			"Get authenticated variable data are not the "
			"same as it set.");
			return FWTS_ERROR;
		}
	}

	fwts_passed(fw, "Update authenticated variable tests passed.");

	return FWTS_OK;
}

/*
 * After updated, set the old data and timestamp authenticated variable,
 * AuthVarCreate, expect EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test6(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Authenticated variable test with old authenticated variable passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetOldAuthVar",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetOldAuthVar",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * Delete the test authenticated variable.
 */
static int uefirtauthvar_test7(fwts_framework *fw)
{
	long ioret;

	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint64_t status;
	uint32_t attributestest;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarDel), AuthVarDel, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIDelAuthVar",
			"Failed to delete authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		if (status == EFI_NOT_FOUND) {
			fwts_passed(fw, "Delete authenticated variable tests passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFIDelAuthVar",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFIDelAuthVar",
		"Failed to delete authenticated variable still get the test"
		"authenticated variable.");

	return FWTS_ERROR;
}

/*
 * Set the authenticated variable with invalid modified data,
 * expect EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test8(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarModData), AuthVarModData, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Set authenticated variable test with invalid modified data passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetAuthVarInvalidData",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetAuthVarInvalidData",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * Set the authenticated variable with invalid modified timestamp, expect
 * EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test9(fwts_framework *fw)
{
	long ioret;
	uint64_t status;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarModTime), AuthVarModTime, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Set authenticated variable test with invalid modified timestamp passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetAuthVarInvalidTime",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetAuthVarInvalidTime",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * Set the authenticated variable with different guid, expect
 * EFI_SECURITY_VIOLATION returned.
 */
static int uefirtauthvar_test10(fwts_framework *fw)
{
	long ioret;
	uint64_t status;
	EFI_GUID gtestguiddiff = TEST_GUID1;

	ioret = setvar(&gtestguiddiff, attributes, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Set authenticated variable test with different guid passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetAuthVarDiffGuid",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetAuthVarDiffGuid",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * The authenticated variable is followed EFI_VARIABLE_AUTHENTICATION_2 descriptor,
 * set the authenticated variable with invalid
 * EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS instead of
 * EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute should
 * return EFI_SECURITY_VIOLATION.
 */
static int uefirtauthvar_test11(fwts_framework *fw)
{
	long ioret;

	uint64_t status;
	uint32_t attr = FWTS_UEFI_VAR_NON_VOLATILE |
			FWTS_UEFI_VAR_BOOTSERVICE_ACCESS |
			FWTS_UEFI_VAR_RUNTIME_ACCESS |
			FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS;

	ioret = setvar(&gtestguid, attr, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		if (status == EFI_SECURITY_VIOLATION) {
			fwts_passed(fw, "Set authenticated variable test with invalid attributes passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFISetAuthVarInvalidAttr",
			"Set authenticated variable fail");
			fwts_uefi_print_status_info(fw, status);
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFISetAuthVarInvalidAttr",
		"Set authenticated variable expected fail but success");

	return FWTS_ERROR;
}

/*
 * Set the authenticated variable with both EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS
 * and the EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attributes, firmware
 * should return EFI_INVALID_PARAMETER.
 */
static int uefirtauthvar_test12(fwts_framework *fw)
{
	long ioret;

	uint64_t status;
	uint32_t attr_both = attributes | FWTS_UEFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS;

	ioret = setvar(&gtestguid, attr_both, sizeof(AuthVarCreate), AuthVarCreate, &status);

	if (ioret == -1) {
		if (status == EFI_INVALID_PARAMETER) {
			fwts_passed(fw, "Test with both authenticated attributes are set passed.");

			return FWTS_OK;
		}
		if (status == EFI_OUT_OF_RESOURCES) {
			fwts_uefi_print_status_info(fw, status);
			fwts_skipped(fw,
				"Run out of resources for SetVariable "
				"UEFI runtime interface: cannot test.");
			fwts_advice(fw,
				"Firmware may reclaim some resources "
				"after rebooting. Reboot and test "
				"again may be helpful to continue "
				"the test.");
			return FWTS_SKIP;
		}
		fwts_failed(fw, LOG_LEVEL_MEDIUM,
			"UEFIRuntimeSetVariable",
			"Failed to set variable with UEFI "
			"runtime service.");

		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFSetAuthVarWithBothAuthAttr",
		"Should not pass setting the authenticated variable "
		"with both authenticated attributes are set.");
	return FWTS_ERROR;
}

/*
 * Test with setting and deleting another authenticated variable,
 * after previous test authenticated variable was deleted.
 */
static int uefirtauthvar_test13(fwts_framework *fw)
{
	long ioret;

	uint8_t data[getvar_buf_size];
	uint64_t getdatasize = sizeof(data);
	uint64_t status;
	uint32_t attributestest;
	size_t i;

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarCreateDiff), AuthVarCreateDiff, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFISetAuthVarDiff",
			"Failed to set authenticated variable with UEFI "
			"runtime service.");

		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFISetAuthVarDiff",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}
	if (getdatasize != sizeof(AuthVarCreateData)) {
		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFISetAuthVarDiff",
			"Get authenticated variable data size is not the "
			"same as it set.");
	}
	for (i = 0; i < sizeof(AuthVarCreateData); i++) {
		if (data[i] != AuthVarCreateData[i]) {
			fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFISetAuthVarDiff",
			"Get authenticated variable data are not the "
			"same as it set.");
		return FWTS_ERROR;
		}
	}

	fwts_passed(fw, "Set authenticated variable created by different key test passed.");

	ioret = setvar(&gtestguid, attributes, sizeof(AuthVarDelDiff), AuthVarDelDiff, &status);

	if (ioret == -1) {
		int supcheck = check_fw_support(fw, status);

		if (supcheck != FWTS_OK)
			return supcheck;

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIDelAuthVarDiff",
			"Failed to delete authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	ioret = getvar(&gtestguid, &attributestest, &getdatasize, data, &status);
	if (ioret == -1) {
		if (status == EFI_NOT_FOUND) {
			fwts_passed(fw, "Delete authenticated variable created by different key test passed.");
			return FWTS_OK;
		}

		fwts_failed(fw, LOG_LEVEL_HIGH,
			"UEFIDelAuthVarDiff",
			"Failed to get authenticated variable with UEFI "
			"runtime service.");
		fwts_uefi_print_status_info(fw, status);
		return FWTS_ERROR;
	}

	fwts_failed(fw, LOG_LEVEL_HIGH,
		"UEFIDelAuthVarDiff",
		"Failed to delete authenticated variable still get the test"
		"authenticated variable.");

	return FWTS_ERROR;
}

static fwts_framework_minor_test uefirtauthvar_tests[] = {
	{ uefirtauthvar_test1, "Create authenticated variable test." },
	{ uefirtauthvar_test2, "Authenticated variable test with the same authenticated variable." },
	{ uefirtauthvar_test3, "Authenticated variable test with another valid authenticated variable." },
	{ uefirtauthvar_test4, "Append authenticated variable test." },
	{ uefirtauthvar_test5, "Update authenticated variable test." },
	{ uefirtauthvar_test6, "Authenticated variable test with old authenticated variable." },
	{ uefirtauthvar_test7, "Delete authenticated variable test." },
	{ uefirtauthvar_test8, "Authenticated variable test with invalid modified data." },
	{ uefirtauthvar_test9, "Authenticated variable test with invalid modified timestamp." },
	{ uefirtauthvar_test10, "Authenticated variable test with different guid." },
	{ uefirtauthvar_test11, "Authenticated variable test with invalid attributes." },
	{ uefirtauthvar_test12, "Test with both authenticated attributes are set." },
	{ uefirtauthvar_test13, "Set and delete authenticated variable created by different key test." },
	{ NULL, NULL }
};

static fwts_framework_ops uefirtauthvar_ops = {
	.description     = "Authenticated variable tests.",
	.init            = uefirtauthvar_init,
	.deinit          = uefirtauthvar_deinit,
	.minor_tests     = uefirtauthvar_tests,
};

FWTS_REGISTER("uefirtauthvar", &uefirtauthvar_ops, FWTS_TEST_ANYTIME, FWTS_FLAG_TEST_UEFI | FWTS_FLAG_UNSAFE | FWTS_FLAG_ROOT_PRIV)