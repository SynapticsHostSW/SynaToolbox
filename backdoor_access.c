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

#define VERSION "1.1"

#define SYNA_TOOL_BOX

#define INPUT_PATH "/sys/class/input/input"
#define NUMBER_OF_INPUTS_TO_SCAN 20

#define OPEN_FILENAME "rmidev/open"
#define DATA_FILENAME "rmidev/data"
#define RELEASE_FILENAME "rmidev/release"
#define DETECT_FILENAME "buildid"

#define PAGES_TO_SERVICE 256
#define PDT_START 0x00E9
#define PDT_END 0x000A
#define PDT_ENTRY_SIZE 0x0006

#define MAX_BUFFER_LEN 1024
#define MAX_STRING_LEN 256

#define WINDOW_SIZE (1 << 6)
#define MASK_6BIT 0x003F

struct pdt_entry {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

static char mySensor[MAX_STRING_LEN];
static char input_detect[MAX_STRING_LEN];
static char rmidev_open[MAX_STRING_LEN];
static char rmidev_data[MAX_STRING_LEN];
static char rmidev_release[MAX_STRING_LEN];

static unsigned short f01_ctrl_base_addr;
static unsigned short f01_data_base_addr;
static unsigned short f81_data_base_addr;

static int fd;

void backdoor_access_usage(void)
{
	printf("\t[-a {address in hex}] [-l {length to read}] [-d {data to write}] [-o {offset to ram backdoor}] [-r] [-w] [-x]\n");

	return;
}


static void usage(char *name)
{
	printf("Version %s\n", VERSION);
	printf("Usage: %s [-a {address in hex}] [-l {length to read}] [-d {data to write}] [-o {offset to ram backdoor}] [-r] [-w] [-x]\n", name);

	return;
}
/*
static void WriteValueToFd(int fd, unsigned int value)
{
	int retval;
	char buf[MAX_BUFFER_LEN];

	snprintf(buf, MAX_BUFFER_LEN, "%u\n", value);

	lseek(fd, 0, SEEK_SET);
	retval = write(fd, buf, strlen(buf));
	if (retval != strlen(buf)) {
		printf("error: failed to write value to file\n");
		close(fd);
		exit(EIO);
	}

	return;
}
*/
static void ErrorExit(int error_code)
{
	close(fd);

	exit(error_code);

	return;
}

static void ReadFromFd(unsigned char *buf, unsigned short byte_address, unsigned int byte_length)
{
	int retval;

	lseek(fd, byte_address, SEEK_SET);
	retval = read(fd, buf, byte_length);
	if (retval != byte_length) {
		printf("error: failed to read data\n");
		printf("address = 0x%04x, length = %d\n", byte_address, byte_length);
		ErrorExit(EIO);
	}

	return;
}

static void WriteToFd(unsigned char *buf, unsigned short byte_address, unsigned int byte_length)
{
	int retval;

	lseek(fd, byte_address, SEEK_SET);
	retval = write(fd, buf, byte_length);
	if (retval != byte_length) {
		printf("error: failed to write data\n");
		printf("address = 0x%04x, length = %d\n", byte_address, byte_length);
		ErrorExit(EIO);
	}

	return;
}

static void CheckForFile(char *filename)
{
	int retval;
	struct stat st;

	retval = stat(filename, &st);
	if (retval) {
		printf("error: %s does not appear to exist\n", filename);
		exit(ENODEV);
	}

	return;
}

static void CheckForFiles(void)
{
	CheckForFile(rmidev_open);
	CheckForFile(rmidev_data);
	CheckForFile(rmidev_release);

	return;
}

static void ScanPDT(unsigned char fn_number)
{
	unsigned short page_number;
	unsigned short pdt_entry_addr;
	struct pdt_entry entry;

	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END; pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			ReadFromFd((unsigned char *)&entry, pdt_entry_addr, sizeof(entry));

			if (entry.fn_number == 0)
				break;

			switch (entry.fn_number) {
			case 0x01:
				f01_ctrl_base_addr = entry.ctrl_base_addr | (page_number << 8);
				f01_data_base_addr = entry.data_base_addr | (page_number << 8);
				break;
			case 0x81:
				f81_data_base_addr = entry.data_base_addr | (page_number << 8);
				break;
			default:
				break;
			}

			if (entry.fn_number == fn_number)
				return;
		}
	}

	printf("error: function 0x%02x not found\n", fn_number);
	ErrorExit(ENODEV);

	return;
}

#ifdef SYNA_TOOL_BOX
int backdoor_access_main(int argc, char* argv[])
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
	unsigned int temp;
	unsigned int index = 0;
	unsigned int word_length = 1;
	unsigned short word_address = 0;
	unsigned short ram_window_addr;
	unsigned short ram_data_addr;
	unsigned short window_num;
	unsigned char xor = 0;
	unsigned char reg_offset = 0;
	unsigned char read_write = 0; /* 0 = read, 1 = write */
	unsigned char byte_offset;
	unsigned char word_offset;
	unsigned char bytes_to_process;
	unsigned char words_to_process;
	unsigned char bytes_to_write;
	unsigned char buf[2];
	unsigned char rw_data[MAX_BUFFER_LEN] = {0};
	char data_input[MAX_STRING_LEN] = {0};
	char next_value[3] = {0};
	char *strptr;
	struct stat st;

#ifndef SYNA_TOOL_BOX
	if (argc == 1) {
		usage(argv[0]);
		exit(EINVAL);
	}
#endif

	for (temp = 0; temp < NUMBER_OF_INPUTS_TO_SCAN; temp++) {
		memset(input_detect, 0x00, MAX_STRING_LEN);
		snprintf(input_detect, MAX_STRING_LEN, "%s%d/%s", INPUT_PATH, temp, DETECT_FILENAME);
		retval = stat(input_detect, &st);
		if (retval == 0) {
			snprintf(mySensor, MAX_STRING_LEN, "%s%d", INPUT_PATH, temp);
			found = 1;
			break;
		}
	}

	if (!found) {
		printf("error: input driver not found\n");
		exit(ENODEV);
	}

	while (this_arg < argc) {
		if (!strcmp((const char *)argv[this_arg], "-r")) {
			read_write = 0;
		} else if (!strcmp((const char *)argv[this_arg], "-w")) {
			read_write = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-a")) {
			this_arg++;
			word_address = (unsigned short)strtoul(argv[this_arg], NULL, 0);
		} else if (!strcmp((const char *)argv[this_arg], "-l")) {
			this_arg++;
			word_length = (unsigned int)strtoul(argv[this_arg], NULL, 0);
		} else if (!strcmp((const char *)argv[this_arg], "-d")) {
			this_arg++;
			temp = strlen(argv[this_arg]) + 1;
			if (temp > MAX_STRING_LEN)
				temp = MAX_STRING_LEN;
			memcpy(data_input, argv[this_arg], temp);
		} else if (!strcmp((const char *)argv[this_arg], "-x")) {
			xor = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-o")) {
			this_arg++;
			reg_offset = (unsigned int)strtoul(argv[this_arg], NULL, 0);
		} else {
#ifdef SYNA_TOOL_BOX
			backdoor_access_usage();
#else
			usage(argv[0]);
#endif
			printf("error: invalid parameter %s\n", argv[this_arg]);
			exit(EINVAL);
		}
		this_arg++;
	}

	snprintf(rmidev_open, MAX_STRING_LEN, "%s/%s", mySensor, OPEN_FILENAME);
	snprintf(rmidev_data, MAX_STRING_LEN, "%s/%s", mySensor, DATA_FILENAME);
	snprintf(rmidev_release, MAX_STRING_LEN, "%s/%s", mySensor, RELEASE_FILENAME);

	CheckForFiles();
/*
	fd = open(rmidev_open, O_WRONLY);
	if (fd < 0) {
		printf("error: failed to open %s\n", rmidev_open);
		exit(EIO);
	}
	WriteValueToFd(fd, 1);
	close(fd);
*/
	fd = open(rmidev_data, O_RDWR);
	if (fd < 0) {
		printf("error: failed to open %s\n", rmidev_data);
		exit(EIO);
	}

	ScanPDT(0x01);
	buf[0] = 0x42;
	WriteToFd(buf, f01_data_base_addr, 1);
	buf[0] = 0xe1;
	WriteToFd(buf, f01_data_base_addr, 1);

	ScanPDT(0x81);
	ram_window_addr = f81_data_base_addr + reg_offset;
	ram_data_addr = f81_data_base_addr + reg_offset + 2;
	buf[0] = 0x4f;
	WriteToFd(buf, ram_window_addr, 1);
	buf[0] = 0x59;
	WriteToFd(buf, ram_window_addr, 1);

	window_num = word_address & ~MASK_6BIT;
	word_offset = word_address & MASK_6BIT;

	if (read_write == 1) {
		if ((strlen(data_input) == 0) || (strlen(data_input) % 2)) {
			printf("error: invalid data format\n");
			ErrorExit(EINVAL);
		}

		strptr = strstr(data_input, "0x");
		if (!strptr) {
			strptr = &data_input[0];
			bytes_to_write = strlen(data_input) / 2;
		} else {
			strptr += 2;
			bytes_to_write = (strlen(data_input) / 2) - 1;
		}

		if (bytes_to_write % 2) {
			printf("error: invalid data format\n");
			ErrorExit(EINVAL);
		}

		word_length = bytes_to_write / 2;

		while (bytes_to_write) {
			memcpy(next_value, strptr, 2);
			rw_data[index + 1] = (unsigned char)strtoul(next_value, NULL, 16);
			strptr += 2;
			memcpy(next_value, strptr, 2);
			rw_data[index] = (unsigned char)strtoul(next_value, NULL, 16);
			strptr += 2;
			index += 2;
			bytes_to_write -= 2;
		}
	}

	if ((word_address + word_length) > (1 << 16)) {
		printf("error: off address boundary\n");
		ErrorExit(EINVAL);
	}

	index = 0;

	while (word_length) {
		buf[0] = (unsigned char)window_num | xor;
		buf[1] = (unsigned char)(window_num >> 8);
		WriteToFd(buf, ram_window_addr, 2);

		if ((word_offset + word_length) > WINDOW_SIZE)
			words_to_process = WINDOW_SIZE - word_offset;
		else
			words_to_process = word_length;

		byte_offset = word_offset * 2;
		bytes_to_process = words_to_process * 2;

		if (read_write == 1) {
			WriteToFd(&rw_data[index], ram_data_addr + byte_offset, bytes_to_process);
			index += bytes_to_process;
		} else {
			ReadFromFd(rw_data, ram_data_addr + byte_offset, bytes_to_process);
			for(temp = 0; temp < bytes_to_process; temp += 2)
				printf("word %4d = 0x%02x%02x\n", index++, rw_data[temp + 1], rw_data[temp]);
		}

		word_length -= words_to_process;
		window_num += WINDOW_SIZE;
		word_offset = 0;
	}

	ReadFromFd(buf, f01_ctrl_base_addr, 1);
	buf[0] |= (1 << 7);
	WriteToFd(buf, f01_ctrl_base_addr, 1);

	close(fd);
/*
	fd = open(rmidev_release, O_WRONLY);
	if (fd < 0) {
		printf("error: failed to open %s\n", rmidev_release);
		exit(EIO);
	}
	WriteValueToFd(fd, 1);
	close(fd);
*/
	return 0;
}
