BEFORE ANY LATENCY MEASUREMENT IS ATTEMPTED
-------------------------------------------
- Be sure that the system has been rebooted, the configuration commands 
have been run, and that ls /dev/rio* returns at least one file.


- If a SWITCH is present in the system:
	- Check that enumeration has worked by entering the following
	command: ls /sys/bus/rapidio/devices/ 
		- At least one file with an "S" in the name should be returned 
		by the above command.

- A test can be aborted by using "CTRL-C". This may result in undefined
  operation which may or may not require a reboot.


EXECUTING LATENCY TESTS
-----------------------

After the system has been configured, choose one of the commands below.
Modify the parameters if necessary. For example if your configuration contains
a switch, you need to use the '-w' switch. Also note that message lengths
are limited to 4K

=============================================================================
Loopback Tests
--------------

* Loopback test with a message of length 1K. This uses master port 0 by default
./umsg.pl -c0 -l1K

* Loopback test, run 5 times, with a message of length 1K, on master port 1
./umsg.pl -c0 -l1K -m1 -r5

* Loopback test with switch, with  a 256 byte message, on port 0
./umsg.pl -c0 -l256 -m0 -w

=============================================================================
Paired Test, Receiver

* Run Receiver and listen on mport 0 for a message from Transmitter (destid = 1)
./umsg.pl -c3 -l256 -m0 -d1

* Run Receiver and listen on mport 1 for data from Transmitter (destid = 1)
./umsg.pl -c3 -l256 -m1 -d1

============================================================================
Paired Test, Transmitter

Note: Run Receiver before running transmitter.

* Run Transmitter and listen on mport 0 for data from Receiver (destid = 0)
./umsg.pl -c2 -l2K -m0 -d0

* Run Transmitter and listen on mport 0 for data from Receiver (destid = 2)
./umsg.pl -c2 -l2K -m0 -d2

=============================================================================
Dual Test

* Dual-card test with 4K & source mport 0
./umsg.pl -c1 -l4K -m0 -d2


* Dual-card test with 2K & source mport 1
./umsg.pl -c1 -l2K -m1 -d0

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

-h  Display help

-w  Tsi721(s) are connected through a switch

-l  Length of message to transfer. By default this is in bytes. Use 'K' to indicate
    kilobytes. Limit is 4K.

-d  destination ID for use in the paired and dual tests

-g  Run in 'gdb' (GNU Debugger)

-r  Number of times a test is repeated.  Default is 1.  Only loopback and
    dual-card tests can be repeated, paired tests may only be executed once.

-v Verbose mode: echoes the command-line parameters passed to the umsg program
   from the script.

