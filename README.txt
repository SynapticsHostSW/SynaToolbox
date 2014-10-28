Install SynaToolbox
-------------------
- Power on device.
- Connect device to host via ADB (USB).
- Open DOS prompt on host and go to directory where ADB and SynaToolbox reside.
- Run "adb devices" to ensure connection with device.
- Run "adb root" to have root privileges.
- Run "adb push SynaToolbox /data" to copy SynaToolbox to /data directory on device.
- Run "adb shell chmod 777 /data/SynaToolbox" to make SynaToolbox executable.



Run SynaToolbox
---------------
- Option 1: execute SynaToolbox to bring up menu selection for choosing tool to use.
- Option 2: run SynaToolbox as single-line command as follows.
  SynaToolbox fw_update [parameters...]
  SynaToolbox read_report [parameters...]
  SynaToolbox reg_access [parameters...]
  SynaToolbox data_logger [parameters...]



Firmware Update Tool Parameters and Usage
-----------------------------------------
Parameters
[-b {image_file}] - Name of image file
[-h {ihex_file}] - Name of iHex file
[-ld] - Do lockdown
[-gc] - Write guest code
[-r] - Read config area
[-ui] - UI config area
[-pm] - Permanent config area
[-bl] - BL config area
[-dp] - Display config area
[-f] - Force reflash
[-v] - Verbose output

Usage examples
- Perform reflash using PR1234567.img
   -b PR1234567.img
- Perform reflash using PR1234567.img and do lockdown
   -b PR1234567.img -ld
- Perform reflash using PR1234567.img regardless of PR number of FW on device
   -b PR1234567.img -f
- Write config data from PR1234567.img (parsing config data from image file)
   -b PR1234567.img -ui
- Write guest code from PR1234567.img (parsing guest code data from image file)
   -b PR1234567.img -gc
- Read UI config area
   -r -ui
- Read permanent config area
   -r -pm
- Perform microbootloader mode recovery using PR1234567.iHex.hex
   -h PR1234567.iHex.hex

Procedures for checking whether to proceed with reflash
- If [-f] flag is set, proceed with reflash.
- If device is in flash programming (bootloader) mode, proceed with reflash.
- If PR number of new FW image is greater than PR number of FW on device,
  proceed with reflash.
- If PR number of new FW image is equal to PR number of FW on device, check
  config ID information. If config ID of new FW image is greater than config
  ID of FW on device, proceed with updating UI config data only.
- Otherwise, no reflash is performed.



Read Report Tool Parameters and Usage
-------------------------------------
Parameters
[-n] - number of report readings to take
[-c] - display output in 2D format

Usage examples
- Read report type 20 once
   20
- Read report type 3 once and display in 2D format
   3 -c
- Read report type 2 15 times and display in 2D format
   2 -n 15 -c



Register Access Tool Parameters and Usage
-----------------------------------------
Parameters
[-a {address in hex}] - Start address (16-bit) for reading/writing
[-o {register offset}] - Offset of register from start address to do read/write from
[-l {length to read}] - Length in bytes to read
[-d {data to write}] - Data (MSB = first byte to write) to write
[-r] - Read for number of bytes set with -l parameter
[-w] - Write data set with -d parameter

Usage examples
- Read five bytes of data starting from third register from address 0x048a
   -a 0x048a -o 3 -l 5 -r
- Write 0x11 0x22 0x33 to address 0x048a starting with 0x11
   -a 0x048a -d 0x112233 -w



Data Logger Tool Parameters and Usage
-------------------------------------
Parameters
[-a {address in hex}] - Start address (16-bit) for data logging
[-l {length to read}] - Length in bytes to read for each interrupt instance
[-m {interrupt mask}] - Interrupt status bit(s) to log data for
[-t {time duration}] - Amount of time in seconds to log data for

Usage example
- Read seven bytes of data starting from address 0x0006 for each 0x04 interrupt for 10 seconds
   -a 0x0006 -l 7 -m 0x04 -t 10

Logged data stored in /data/data_logger_output.txt.
Multiple interrupt status bits may be OR'ed together to form interrupt mask.
