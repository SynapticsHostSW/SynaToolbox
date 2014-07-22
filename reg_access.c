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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define VERSION "1.6"

#define SYNA_TOOL_BOX

#define INPUT_PATH "/sys/class/input/input"
#define NUMBER_OF_INPUTS_TO_SCAN 20

#define OPEN_FILENAME "rmidev/open"
#define DATA_FILENAME "rmidev/data"
#define RELEASE_FILENAME "rmidev/release"
#define DETECT_FILENAME "buildid"

#define MAX_BUFFER_LEN 256
#define MAX_STRING_LEN 256

char mySensor[MAX_STRING_LEN];
char input_detect[MAX_STRING_LEN];

char rmidev_open[MAX_STRING_LEN];
char rmidev_data[MAX_STRING_LEN];
char rmidev_release[MAX_STRING_LEN];

unsigned char *w_data;
unsigned char *r_data;

unsigned char read_write = 0; /* 0 = read, 1 = write */
unsigned short address = 0;
unsigned int length = 1;
unsigned int offset = 0;

void reg_access_usage(void)
{
	printf("\t[-a {address in hex}] [-o {offset}] [-l {length to read}] [-d {data to write}] [-r] [-w]\n");

	return;
}

static void usage(char *name)
{
	printf("Version %s\n", VERSION);
	printf("Usage: %s [-a {address in hex}] [-o {offset}] [-l {length to read}] [-d {data to write}] [-r] [-w]\n", name);

	return;
}

static void error_exit(error_code)
{
	if (w_data)
		free(w_data);

	if (r_data)
		free(r_data);

	exit(error_code);

	return;
}
/*
static void writevaluetofd(int fd, unsigned int value)
{
	int numBytesWritten = 0;
	char buf[MAX_BUFFER_LEN];

	snprintf(buf, MAX_BUFFER_LEN, "%u\n", value);

	lseek(fd, 0, SEEK_SET);
	numBytesWritten = write(fd, buf, strlen(buf));
	if (numBytesWritten != strlen(buf)) {
		printf("error: failed to write all bytes to file\n");
		close(fd);
		error_exit(EIO);
	}

	return;
}
*/
static int CheckOneFile(char* filename)
{
	int retval;
	struct stat st;

	retval = stat(filename, &st);

	if (retval)
		printf("error: %s does not appear to exist\n", filename);

	return retval;
}

static int CheckFiles(void)
{
	int retval;

	retval = CheckOneFile(rmidev_open);
	if (retval)
		return retval;

	retval = CheckOneFile(rmidev_data);
	if (retval)
		return retval;

	retval = CheckOneFile(rmidev_release);
	if (retval)
		return retval;

	return 0;
}

#ifdef SYNA_TOOL_BOX
int reg_access_main(int argc, char* argv[])
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
	int fd;
	unsigned long temp;
	unsigned long index;
	unsigned char bytes_to_write;
	char data_input[MAX_STRING_LEN] = {0};
	char next_value[3] = {0};
	char *strptr;
	struct stat st;

#ifndef SYNA_TOOL_BOX
	if (argc == 1) {
		usage(argv[0]);
		error_exit(EINVAL);
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
		error_exit(ENODEV);
	}

	while (this_arg < argc) {
		if (!strcmp((const char *)argv[this_arg], "-r")) {
			read_write = 0;
		} else if (!strcmp((const char *)argv[this_arg], "-w")) {
			read_write = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-a")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			address = (unsigned short)temp;
		} else if (!strcmp((const char *)argv[this_arg], "-o")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			offset = (unsigned int)temp;
		} else if (!strcmp((const char *)argv[this_arg], "-l")) {
			this_arg++;
			temp = strtoul(argv[this_arg], NULL, 0);
			length = (unsigned int)temp;
		} else if (!strcmp((const char *)argv[this_arg], "-d")) {
			this_arg++;
			memcpy(data_input, argv[this_arg], strlen(argv[this_arg]));
		} else {
#ifdef SYNA_TOOL_BOX
			reg_access_usage();
#else
			usage(argv[0]);
#endif
			printf("error: invalid parameter %s\n", argv[this_arg]);
			error_exit(EINVAL);
		}
		this_arg++;
	}

	snprintf(rmidev_open, MAX_STRING_LEN, "%s/%s", mySensor,
			OPEN_FILENAME);
	snprintf(rmidev_data, MAX_STRING_LEN, "%s/%s", mySensor,
			DATA_FILENAME);
	snprintf(rmidev_release, MAX_STRING_LEN, "%s/%s", mySensor,
			RELEASE_FILENAME);

	if (CheckFiles())
		error_exit(ENODEV);
/*
	fd = open(rmidev_open, O_WRONLY);
	if (fd < 0) {
		printf("error: failed to open %s\n", rmidev_open);
		error_exit(EIO);
	}
	writevaluetofd(fd, 1);
	close(fd);
*/
	if (read_write == 1) {
		if ((strlen(data_input) == 0) || (strlen(data_input) % 2)) {
			printf("error: invalid data format\n");
			error_exit(EIO);
		}

		strptr = strstr(data_input, "0x");
		if (!strptr) {
			strptr = &data_input[0];
			bytes_to_write = strlen(data_input) / 2;
		} else {
			strptr += 2;
			bytes_to_write = (strlen(data_input) / 2) - 1;
		}

		w_data = malloc(bytes_to_write + offset);
		if (!w_data) {
			printf("error: failed to allocate w_data buffer\n");
			error_exit(ENOMEM);
		}

		if (offset) {
			fd = open(rmidev_data, O_RDONLY);

			if (fd < 0) {
				printf("error: failed to open %s\n", rmidev_data);
				error_exit(EIO);
			}

			lseek(fd, address, SEEK_SET);

			retval = read(fd, w_data, bytes_to_write + offset);
			if (retval != (bytes_to_write + offset)) {
				printf("error: failed to read data\n");
				close(fd);
				error_exit(EIO);
			}

			close(fd);
		}

		temp = bytes_to_write;
		index = offset;
		while (temp) {
			memcpy(next_value, strptr, 2);
			w_data[index] = (unsigned char)strtoul(next_value, NULL, 16);
			strptr += 2;
			index++;
			temp--;
		}

		fd = open(rmidev_data, O_WRONLY);
		if (fd < 0) {
			printf("error: failed to open %s\n", rmidev_data);
			error_exit(EIO);
		}

		lseek(fd, address, SEEK_SET);

		retval = write(fd, w_data, bytes_to_write + offset);
		if (retval != (bytes_to_write + offset)) {
			printf("error: failed to write data\n");
			close(fd);
			error_exit(EIO);
		}

		close(fd);
	} else {
		r_data = malloc(length + offset);
		if (!r_data) {
			printf("error: failed to allocate r_data buffer\n");
			error_exit(ENOMEM);
		}

		fd = open(rmidev_data, O_RDONLY);

		if (fd < 0) {
			printf("error: failed to open %s\n", rmidev_data);
			error_exit(EIO);
		}

		lseek(fd, address, SEEK_SET);

		retval = read(fd, r_data, length + offset);
		if (retval != (length + offset)) {
			printf("error: failed to read data\n");
			close(fd);
			error_exit(EIO);
		}

		close(fd);

		for(temp = offset; temp < (length + offset); temp++)
			printf("data %d = 0x%02x\n", (unsigned int)temp, r_data[temp]);
	}
/*
	fd = open(rmidev_release, O_WRONLY);
	if (fd < 0) {
		printf("error: failed to open %s\n", rmidev_release);
		error_exit(EIO);
	}
	writevaluetofd(fd, 1);
	close(fd);
*/
	if (w_data)
		free(w_data);

	if (r_data)
		free(r_data);

	return 0;
}
