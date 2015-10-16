/*
 * Copyright (C) 2015, Al Stone <ahs3@redhat.com>
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

static int foo_init(fwts_framework *fw)
{
	/* Put pre-test initialistion code here */
	fwts_log_info(fw, "foo_init called");

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do tests
	 */
	return FWTS_OK;
}

static int foo_deinit(fwts_framework *fw)
{
	/* Put post-test de-initialistion code here */
	fwts_log_info(fw, "foo_deinit called");

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do tests
	 */
	return FWTS_OK;
}

static int foo_test1(fwts_framework *fw)
{
	/* Do your test */

	/* Log success or failure */
	fwts_passed(fw, "Test passed, hurrah!");
	/*
	fwts_failed(fw, LOG_LEVEL_HIGH, "ExampleUniqueTestMessageIdentifier", "Test failed!");
	*/

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do test
	 */
	return FWTS_OK;
}

static int foo_test2(fwts_framework *fw)
{
	/* Do your test */

	/* Log success or failure */
	fwts_passed(fw, "Test passed, hurrah!");
	/*
	fwts_failed(fw, LOG_LEVEL_HIGH, "ExampleUniqueTestMessageIdentifier", "Test failed!");
	*/

	/* Returns:
	 *	FWTS_ERROR - failed, abort test
	 *	FWTS_OK    - success, do test
	 */
	return FWTS_OK;
}

/*
 *  Null terminated array of tests to run, in this
 *  scenario, we just have one test.
 */
static fwts_framework_minor_test foo_tests[] = {
	{ foo_test1, "Foo subtest1 name." },
	{ foo_test2, "Foo subtest2 name." },
	{ NULL, NULL }
};

static fwts_framework_ops foo_ops = {
	.description = "Foo test.", /* Simple short description of test */
	.init        = foo_init,
	.deinit      = foo_deinit,
	.minor_tests = foo_tests    /* Array of tests to run */
};

/*
 *   See fwts_framework.h for flags,
 */
FWTS_REGISTER("foo", &foo_ops, FWTS_TEST_ANYTIME,
	      FWTS_FLAG_BATCH | FWTS_FLAG_TEST_ACPI | FWTS_FLAG_TEST_SBBR);
