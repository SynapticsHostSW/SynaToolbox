/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Copyright © 2011, 2012 Synaptics Incorporated. All rights reserved.
 *
 * The information in this file is confidential under the terms
 * of a non-disclosure agreement with Synaptics and is provided
 * AS IS without warranties or guarantees of any kind.
 *
 * The information in this file shall remain the exclusive property
 * of Synaptics and may be the subject of Synaptics patents, in
 * whole or part. Synaptics intellectual property rights in the
 * information in this file are not expressly or implicitly licensed
 * or otherwise transferred to you as a result of such information
 * being made available to you.
 *
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define VERSION "1.4"

#define INPUT_PATH "/sys/class/input/input"
#define NUMBER_OF_INPUTS_TO_SCAN 20
#define MAX_STRING_LEN 256
#define MAX_NUM_ARGS 256
#define BUILDID_FILENAME "buildid"

char driver_path[MAX_STRING_LEN];
char input_number[MAX_STRING_LEN];
char stdin_input[MAX_STRING_LEN];
char *arg[MAX_NUM_ARGS];

extern void fw_update_usage(void);
extern int fw_update_main(int argc, char* argv[]);
extern void read_report_usage(void);
extern int read_report_main(int argc, char* argv[]);
extern void reg_access_usage(void);
extern int reg_access_main(int argc, char* argv[]);
extern void backdoor_access_usage(void);
extern int backdoor_access_main(int argc, char* argv[]);
extern void data_logger_usage(void);
extern int data_logger_main(int argc, char* argv[]);

enum tool {
	FW_UPDATE = 0x01,
	READ_REPORT = 0x02,
	REG_ACCESS = 0x03,
	DATA_LOGGER = 0x04,
	BACKDOOR_ACCESS = 0x81,
};

static void error_exit(error_code)
{
	exit(-error_code);

	return;
}

static void usage(void)
{
	printf("Version %s\n", VERSION);
	printf("1 - Update firmware\n");
	printf("2 - Read test reports\n");
	printf("3 - Access registers\n");
	printf("4 - Log register data\n");
	printf("Enter tool selection: ");

	return;
}

static int parse_args(void)
{
	int count = 0;
	char *token;
	char *state;

	for (token = strtok_r(stdin_input, " ", &state); token != NULL; token = strtok_r(NULL, " ", &state)) {
		arg[count] = token;
		count++;
	}

	return count;
}

static int get_line(void)
{
	int c;
	int count = 0;

	while ((c = fgetc(stdin)) != EOF) {
		if (c == '\n')
			break;

		if (count < (MAX_STRING_LEN - 1)) {
			stdin_input[count] = c;
			count++;
		} else {
			stdin_input[count] = '\0';
			break;
		}
	}

	stdin_input[count] = '\0';

	return count;
}

static void print_tool_usage(int tool)
{
	switch (tool) {
	case FW_UPDATE:
		fw_update_usage();
		break;
	case READ_REPORT:
		read_report_usage();
		break;
	case REG_ACCESS:
		reg_access_usage();
		break;
	case DATA_LOGGER:
		data_logger_usage();
		break;
	case BACKDOOR_ACCESS:
		backdoor_access_usage();
		break;
	}

	return;
}

static void run_tool(int tool, int count)
{
	switch (tool) {
	case FW_UPDATE:
		fw_update_main(count, arg);
		break;
	case READ_REPORT:
		read_report_main(count, arg);
		break;
	case REG_ACCESS:
		reg_access_main(count, arg);
		break;
	case DATA_LOGGER:
		data_logger_main(count, arg);
		break;
	case BACKDOOR_ACCESS:
		backdoor_access_main(count, arg);
		break;
	}

	return;
}

int main(int argc, char* argv[])
{
	int retval;
	int input_num;
	int found = 0;
	int this_arg;
	int tool;
	int count;
	struct stat st;

	for (input_num = 0; input_num < NUMBER_OF_INPUTS_TO_SCAN; input_num++) {
		memset(input_number, 0x00, MAX_STRING_LEN);
		snprintf(input_number, MAX_STRING_LEN, "%s%d/%s", INPUT_PATH, input_num, BUILDID_FILENAME);
		retval = stat(input_number, &st);
		if (retval == 0) {
			snprintf(driver_path, MAX_STRING_LEN, "%s%d", INPUT_PATH, input_num);
			found = 1;
			break;
		}
	}

	if (!found) {
		printf("ERROR: input driver not found\n");
		error_exit(ENODEV);
	}

	if (argc > 1) {
		if (strcmp(argv[1], "fw_update") == 0) {
			tool = FW_UPDATE;
		} else if (strcmp(argv[1], "read_report") == 0) {
			tool = READ_REPORT;
		} else if (strcmp(argv[1], "reg_access") == 0) {
			tool = REG_ACCESS;
		} else if (strcmp(argv[1], "backdoor_access") == 0) {
			tool = BACKDOOR_ACCESS;
		} else if (strcmp(argv[1], "data_logger") == 0) {
			tool = DATA_LOGGER;
		} else {
			printf("ERROR: invalid tool name\n");
			error_exit(EINVAL);
		}

		if (argc == 2) {
			printf("ERROR: tool parameters missing\n");
			print_tool_usage(tool);
			error_exit(EINVAL);
		}

		this_arg = 2;
		count = 0;
		while (this_arg < argc) {
			arg[count] = argv[this_arg];
			this_arg++;
			count++;
		}
		run_tool(tool, count);
		return 0;
	}

	usage();
	get_line();
	parse_args();
	switch (*arg[0]) {
	case '1':
	case '2':
	case '3':
	case '4':
		tool = stdin_input[0] - '0';
		break;
	case '*':
		get_line();
		if ((stdin_input[0] == '8') && (stdin_input[1] == '1')) {
			tool = BACKDOOR_ACCESS;
			break;
		}
	default:
		printf("ERROR: invalid input\n");
		error_exit(EINVAL);
	}

tool_parameters:
	printf("Enter tool parameters (? for help): ");
	get_line();
	count = parse_args();
	if (*arg[0] == '?') {
		print_tool_usage(tool);
		goto tool_parameters;
	} else if (count > 0) {
		run_tool(tool, count);
	}

	return 0;
}
