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
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VERSION "1.5"

#define SYNA_TOOL_BOX

#define INPUT_PATH "/sys/class/input/input"
#define NUMBER_OF_INPUTS_TO_SCAN 20

#define MAX_FRAME_RATE 120

#define PID_FILENAME "rmidb/pid"
#define TERM_FILENAME "rmidb/term"
#define QUERY_BASE_FILENAME "rmidb/query_base_addr"
#define CONTROL_BASE_FILENAME "rmidb/control_base_addr"
#define DATA_BASE_FILENAME "rmidb/data_base_addr"
#define COMMAND_BASE_FILENAME "rmidb/command_base_addr"
#define DATA_FILENAME "rmidev/data"
#define OUTPUT_FILENAME "/data/db_output.txt"
#define DETECT_FILENAME "buildid"

#define MAX_STRING_LEN 256
#define MAX_BUFFER_LEN 256

char mySensor[MAX_STRING_LEN];
char input_detect[MAX_STRING_LEN];

char rmidb_pid[MAX_STRING_LEN];
char rmidb_term[MAX_STRING_LEN];
char rmidb_query_base[MAX_STRING_LEN];
char rmidb_control_base[MAX_STRING_LEN];
char rmidb_data_base[MAX_STRING_LEN];
char rmidb_command_base[MAX_STRING_LEN];
char rmidb_data[MAX_STRING_LEN];
char rmidb_output[MAX_STRING_LEN];

unsigned short r_address = 0;
unsigned char *r_data;
unsigned int r_data_size;
unsigned int r_length = 0;
unsigned int r_duration = 0;
unsigned int r_index = 0;
unsigned int query_base;
unsigned int control_base;
unsigned int data_base;
unsigned int command_base;

int fd_pid;
int fd_term;
int fd_query_base;
int fd_control_base;
int fd_data_base;
int fd_command_base;
int fd_data;
FILE *fp_output;

static void read_value_from_fd(int fd, unsigned int *value);
static void write_value_to_fd(int fd, unsigned int value);

void db_logger_usage(void)
{
	printf("\t[-a {start address}] [-l {length to read}] [-t {time duration}]\n");

	return;
}

static void usage(char *name)
{
	printf("Version %s\n", VERSION);
	printf("Usage: %s [-a {start address}] [-l {length to read}] [-t {time duration}]\n", name);

	return;
}

static void close_all_fds(void)
{
	write_value_to_fd(fd_pid, 0);

	if (fd_pid)
		close(fd_pid);

	if (fd_term)
		close(fd_term);

	if (fd_query_base)
		close(fd_query_base);

	if (fd_control_base)
		close(fd_control_base);

	if (fd_data_base)
		close(fd_data_base);

	if (fd_command_base)
		close(fd_command_base);

	if (fd_data)
		close(fd_data);

	return;
}

static void open_all_fds(void)
{
	fd_pid = open(rmidb_pid, O_WRONLY);
	if (fd_pid < 0) {
		printf("error: failed to open %s\n", rmidb_pid);
		close_all_fds();
		exit(EIO);
	}

	fd_term = open(rmidb_term, O_WRONLY);
	if (fd_term < 0) {
		printf("error: failed to open %s\n", rmidb_term);
		close_all_fds();
		exit(EIO);
	}

	fd_query_base = open(rmidb_query_base, O_RDONLY);
	if (fd_query_base < 0) {
		printf("error: failed to open %s\n", rmidb_query_base);
		close_all_fds();
		exit(EIO);
	}

	fd_control_base = open(rmidb_control_base, O_RDONLY);
	if (fd_control_base < 0) {
		printf("error: failed to open %s\n", rmidb_control_base);
		close_all_fds();
		exit(EIO);
	}

	fd_data_base = open(rmidb_data_base, O_RDONLY);
	if (fd_data_base < 0) {
		printf("error: failed to open %s\n", rmidb_data_base);
		close_all_fds();
		exit(EIO);
	}

	fd_command_base = open(rmidb_command_base, O_RDONLY);
	if (fd_command_base < 0) {
		printf("error: failed to open %s\n", rmidb_command_base);
		close_all_fds();
		exit(EIO);
	}

	fd_data = open(rmidb_data, O_RDWR);
	if (fd_data < 0) {
		printf("error: failed to open %s\n", rmidb_data);
		close_all_fds();
		exit(EIO);
	}

	return;
}

static void error_exit(error_code)
{
	close_all_fds();
	if (r_data)
		free(r_data);
	exit(error_code);

	return;
}

static void read_value_from_fd(int fd, unsigned int *value)
{
	int retval;
	char buf[MAX_BUFFER_LEN] = {0};

	retval = read(fd, buf, MAX_BUFFER_LEN - 1);
	if (retval == -1) {
		printf("error: failed to read from file descriptor\n");
		error_exit(EIO);
	}

	*value = strtoul(buf, NULL, 0);

	return;
}

static void write_value_to_fd(int fd, unsigned int value)
{
	int retval;
	char buf[MAX_BUFFER_LEN] = {0};

	snprintf(buf, MAX_BUFFER_LEN - 1, "%u", value);

	retval = write(fd, buf, strlen(buf) + 1);
	if (retval != ((int)(strlen(buf) + 1))) {
		printf("error: failed to write to file descriptor\n");
		error_exit(EIO);
	}

	return;
}

static int check_one_file(char* filename)
{
	int retval;
	struct stat st;

	retval = stat(filename, &st);

	if (retval)
		printf("error: %s does not appear to exist\n", filename);

	return retval;
}

static int check_files(void)
{
	int retval;

	retval = check_one_file(rmidb_pid);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_term);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_query_base);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_control_base);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_data_base);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_command_base);
	if (retval)
		return retval;

	retval = check_one_file(rmidb_data);
	if (retval)
		return retval;

	return 0;
}

void interrupt_handler(int signum)
{
	int retval;

	if ((signum == SIGIO) && ((r_index + r_length) < r_data_size)) {
		lseek(fd_data, r_address, SEEK_SET);
		retval = read(fd_data, &r_data[r_index], r_length);
		if (retval != r_length) {
			printf("error: failed to read data\n");
			error_exit(EIO);
		}
		r_index += r_length;
	} else if (signum == SIGIO) {
		printf("error: reached end of data buffer\n");
	}

	return;
}

void terminate_handler(int signum)
{
	int retval;
	unsigned int ii;
	unsigned int index = 0;
	unsigned int count = 1;
	unsigned int buffer_size;
	char *snprintf_buf;
	char null_char = 0;

	if (signum != SIGTERM)
		return;

	write_value_to_fd(fd_pid, 0);

	buffer_size = 7 + (5 * r_length) + 2 + 1;

	snprintf_buf = malloc(buffer_size);
	if (!snprintf_buf) {
		printf("error: failed to allocate output line buffer\n");
		error_exit(ENOMEM);
	} else {
		memset(snprintf_buf, 0x00, buffer_size);
	}

	fp_output = fopen(rmidb_output, "w");
	if (!fp_output) {
		printf("error: failed to open %s\n", rmidb_output);
		free(snprintf_buf);
		error_exit(EIO);
	}

	fseek(fp_output, 0, SEEK_SET);

	while (index < r_index) {
		snprintf(&snprintf_buf[0], buffer_size, "%5d :", count++);

		for (ii = 0; ii < r_length; ii++)
			snprintf(&snprintf_buf[7 + (ii * 5)], buffer_size, " 0x%02x", r_data[index + ii]);

		snprintf(&snprintf_buf[7 + (r_length * 5)], buffer_size, "\r\n");

		retval = fwrite(snprintf_buf, 1, strlen(snprintf_buf), fp_output);
		if (retval != ((int)(strlen(snprintf_buf)))) {
			printf("error: failed to write to output file\n");
			fclose(fp_output);
			free(snprintf_buf);
			error_exit(EIO);
		}

		index += r_length;
	}

	retval = fwrite(&null_char, 1, 1, fp_output);
	if (retval != 1) {
		printf("error: failed to write to output file\n");
		fclose(fp_output);
		free(snprintf_buf);
		error_exit(EIO);
	}

	printf("Data logging completed\n");

	fclose(fp_output);
	free(snprintf_buf);
	error_exit(EIO);

	return;
}

#ifdef SYNA_TOOL_BOX
int db_logger_main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	int retval;
#ifdef SYNA_TOOL_BOX
	int this_arg = 0;
#else
	int this_arg = 1;
#endif
	int found = 0;
	unsigned long temp;
	struct stat st;
	struct sigaction interrupt_signal;
	struct sigaction terminate_signal;
	pid_t pid;
	time_t timeout;

#ifndef SYNA_TOOL_BOX
	if (argc == 1) {
		usage(argv[0]);
		exit(EINVAL);
	}
#endif

	for (temp = 0; temp < NUMBER_OF_INPUTS_TO_SCAN; temp++) {
		memset(input_detect, 0x00, MAX_STRING_LEN);
		snprintf(input_detect, MAX_STRING_LEN, "%s%d/%s", INPUT_PATH,
				(unsigned int)temp, DETECT_FILENAME);
		retval = stat(input_detect, &st);
		if (retval == 0) {
			snprintf(mySensor, MAX_STRING_LEN, "%s%d", INPUT_PATH,
					(unsigned int)temp);
			found = 1;
			break;
		}
	}

	if (!found) {
		printf("error: input driver not found\n");
		exit(ENODEV);
	}

	while (this_arg < argc) {
		if (!strcmp((const char *)argv[this_arg], "-a")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			r_address = (unsigned short)temp;
		} else if (!strcmp((const char *)argv[this_arg], "-l")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			r_length = (unsigned int)temp;
		} else if (!strcmp((const char *)argv[this_arg], "-t")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			r_duration = (unsigned int)temp;
		} else {
#ifdef SYNA_TOOL_BOX
			db_logger_usage();
#else
			usage(argv[0]);
#endif
			printf("error: invalid parameter %s\n", argv[this_arg]);
			exit(EINVAL);
		}
		this_arg++;
	}

	if (!r_length || !r_duration) {
#ifdef SYNA_TOOL_BOX
		db_logger_usage();
#else
		usage(argv[0]);
#endif
		exit(EINVAL);
	}

	pid = getpid();

	interrupt_signal.sa_handler = interrupt_handler;
	sigemptyset(&interrupt_signal.sa_mask);
	interrupt_signal.sa_flags = 0;
	sigaction(SIGIO, &interrupt_signal, NULL);

	terminate_signal.sa_handler = terminate_handler;
	sigemptyset(&terminate_signal.sa_mask);
	terminate_signal.sa_flags = 0;
	sigaction(SIGTERM, &terminate_signal, NULL);

	snprintf(rmidb_pid, MAX_STRING_LEN, "%s/%s", mySensor, PID_FILENAME);
	snprintf(rmidb_term, MAX_STRING_LEN, "%s/%s", mySensor, TERM_FILENAME);
	snprintf(rmidb_query_base, MAX_STRING_LEN, "%s/%s", mySensor, QUERY_BASE_FILENAME);
	snprintf(rmidb_control_base, MAX_STRING_LEN, "%s/%s", mySensor, CONTROL_BASE_FILENAME);
	snprintf(rmidb_data_base, MAX_STRING_LEN, "%s/%s", mySensor, DATA_BASE_FILENAME);
	snprintf(rmidb_command_base, MAX_STRING_LEN, "%s/%s", mySensor, COMMAND_BASE_FILENAME);
	snprintf(rmidb_data, MAX_STRING_LEN, "%s/%s", mySensor, DATA_FILENAME);
	snprintf(rmidb_output, MAX_STRING_LEN, "%s", OUTPUT_FILENAME);

	if (check_files())
		exit(ENODEV);

	open_all_fds();

	r_data_size = r_length * MAX_FRAME_RATE * r_duration;
	r_data = malloc(r_data_size);
	if (!r_data) {
		printf("error: failed to allocate data buffer\n");
		close_all_fds();
		exit(ENOMEM);
	}

	read_value_from_fd(fd_query_base, &query_base);
	read_value_from_fd(fd_control_base, &control_base);
	read_value_from_fd(fd_data_base, &data_base);
	read_value_from_fd(fd_command_base, &command_base);

	lseek(fd_data, 0, SEEK_SET);

	write_value_to_fd(fd_pid, pid);

	printf("Data logging started\n");

	timeout = time(0);
	while ((time(0) - timeout) < r_duration)
		sleep(1);
	write_value_to_fd(fd_term, 1);

	while (1)
		sleep(1);

	return 0;
}
