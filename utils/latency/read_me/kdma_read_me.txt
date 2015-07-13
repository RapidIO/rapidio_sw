BEFORE ANY LATENCY MEASUREMENT IS ATTEMPTED
-------------------------------------------
- Be sure that the system has been rebooted, the configuration commands 
have been run, and that ls /dev/rio* returns at least one file.


- If a SWITCH is present in the system:
	- Check that enumeration has worked by entering the following
	command: ls /sys/bus/rapidio/devices/ 
		- At least one file with an ":s:" in the name should be returned 
		by the above command.
	- IMPORTANT: the first latency measurement  run after rebooting MUST 
	be executed with the -W (switch present) option

- NOTE: After a "Loopback" test has been run without a switch, the link partner
with be in PORT_UNINIT state.
	- A test with "-c 1" (Paired, RX) or "-c 3" (Dual) must be run to 
	take the Tsi721 out of loopback, 

- A test can be aborted by using "CTRL-c". This will result in undefined
  operation which may or may not require a reboot.


EXECUTING LATENCY TESTS
-----------------------

After the system has been configured, choose one of the commands below.
Modify the parameters if necessary.  Note that each command is shown with
two options.  The top command is used to execute the test, while the bottom
one may be used for debugging.

Note: For a "paired" configuration, always start the "RX" 
before starting the "TX".

PARAMETERS ASSUME NO RAPIDIO SWITCH.  TO ADD A RAPIDIO SWITCH BETWEEN THE
TSI721, APPEND THE -W OPTION.

=============================================================================
Loopback Test

./kdma -c 0 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 5 -I 0x200000 -R 0x20000000 -t

gdb --args ./kdma -c 0 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 5 -I 0x200000 -R 0x20000000 -d -t

=============================================================================
Paired Test, Receiver

./kdma -c 2 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 

gdb --args ./kdma -c 2 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 -d

============================================================================
Paired Test, Transmitter

./kdma -c 1 -M 1 -D1 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 -t

gdb --args ./kdma -c 1 -M 1 -D1 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 -d -t


=============================================================================
Dual Test

./kdma -c 3 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 -t

gdb --args ./kdma -c 3 -M 0 -D0 -A 0x20000000 -S 0x100000 -O 0 -T 1 -I 0x200000 -R 0x20000000 -d -t

=============================================================================

TEST PARAMETERS
---------------

Note on Parameters
-Mx - 'x' selects the Tsi721 based on master port.  To list available Tsi721's,
     execute the following command: ls /dev/rio*
     The 'x' is the number at the end of the file name(s).
     If no devices appear, execute: modprobe rio-mport-cdev

-c - Test configuration, encoded as follows:
	0 - Loopback configuration
	1 - Transmitter for paired Tsi721 on this platform
            *** Receiver must be started up before transmitter!!! ***
	2 - Receiver for paired Tsi721 on this platform
            *** Receiver must be started up before transmitter!!! ***
	3 - Dual Tsi721's in this platform.
            *** There must be two Tsi721's present to run this test ***

-k - Use kernel mode buffers

-v - Disable verification of transfers

-T - Number of times a test is repeated.  Default is 1.  Only loopback and
     dual mode tests can be repeated, paired tests may only be executed once.

-W - Tsi721(s) are connected through a sWitch

-t - Use separate threads to measure latency.  This measures latency from 
     the time of the DMA transaction starting, to the time of the last 
     byte received.  Without this option, the time measured for shorter 
     transactions is the time required to complete the DMA transaction and
     return execution to the calling process, which is longer than the 
     actual transfer.
