BEFORE ANY LATENCY MEASUREMENT IS ATTEMPTED
-------------------------------------------
- Be sure that the system has been rebooted, the configuration commands 
have been run, and that ls /dev/rio* returns at least one file.


- If a SWITCH is present in the system:
	- Check that enumeration has worked by entering the following
	command: ls /sys/bus/rapidio/devices/ 
		- At least one file with an "S" in the name should be returned 
		by the above command.

- A test can be aborted by using "CTRL-c". This will result in undefined
  operation which may or may not require a reboot.


EXECUTING LATENCY TESTS
-----------------------

After the system has been configured, choose one of the commands below.
Modify the parameters if necessary. For example, if your configuration
includes a switch between the various endpoints, then add the '-w' option. 

=============================================================================
Loopback Tests
--------------

* Loopback test with 1K of data. This uses master port 0 by default:
./udma.pl -c 0 -l 1K

* Loopback test with 1M of data, on master port 1:
./udma.pl -c 0 -l 1M -m 1

* Loopback test with switch, with 256 bytes of data, on port 0
./udma.pl -c 0 -l 256 -m 0 -w

=============================================================================
Paired Test, Receiver

* Run Receiver and listen on mport 0 for data from Transmitter (destid = 1)
./udma.pl -c 3 -l 256 -m 0 -d 1

* Run Receiver and listen on mport 1 for data from Transmitter (destid = 1)
./udma.pl -c 3 -l 256 -m 1 -d 1

============================================================================
Paired Test, Transmitter

Note: Run Receiver before running transmitter. 

* Run Transmitter and send on mport 0 data to Receiver (destid = 0)
./udma.pl -c 2 -l 256 -d 0

* Run Transmitter and send on mport 0 data to Receiver (destid = 2)
./udma.pl -c 2 -l 256 -d 2 

=============================================================================
Dual Test

* Dual-card test with 4K & source mport 0
./udma.pl -c 1 -l 4K -m 0


* Dual-card test with 8K & source mport 1
./udma.pl -c 1 -l 8K -m 1

=============================================================================

TEST PARAMETERS
---------------

Note on Parameters
-mx - 'x' selects the Tsi721 based on master port.  To list available Tsi721's,
     execute the following command: ls /dev/rio*
     The 'x' is the number at the end of the file name(s).
     If no devices appear, execute: modprobe rio-mport-cdev

-c - Test configuration, encoded as follows:
	0 - Loopback configuration
	1 - Dual Tsi721's in this platform.
            *** There must be two Tsi721's present to run this test ***
	2 - Transmitter for paired Tsi721 on this platform
            *** Receiver must be started up before transmitter!!! ***
	3 - Receiver for paired Tsi721 on this platform
            *** Receiver must be started up before transmitter!!! ***

-w - Tsi721(s) are connected through a switch

-l  Length of data to transfer. By default this is in bytes. Use 'K' to indicate
    kilobytes and 'M' to indicate megabytes.

-d  destination ID for use in the paired tests

