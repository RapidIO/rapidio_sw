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

- The server can be aborted by using "CTRL-c". This will result in undefined
  operation which may or may not require a reboot.


EXECUTING LATENCY TESTS
-----------------------

After the system has been configured, choose one of the commands below.
Modify the parameters if necessary.  Note that each command is shown with
two options.  The top command is used to execute the test, while the bottom
one may be used for debugging.

Note: For a "paired" configuration, always start the "Server" 
before starting the "Client".

PARAMETERS ASSUME NO RAPIDIO SWITCH.  TO ADD A RAPIDIO SWITCH BETWEEN THE
TSI721, APPEND THE -W OPTION.

=============================================================================
Loopback Test

NOTE: When running the loopback test, the server must be configured as follows:
- "loopback" patch IS applied
- boot command line paramter is "rapidio.hdid=1"

./kmsg -c 0 -M 0 -D 0 -S 0x1000 -T 1 -C 3335

gdb --args ./kmsg -c 0 -M 1 -D 1 -S 0x1000 -T 1 -C 3335 -d

=============================================================================
Paired Test, Server  

NOTE: When running the paired test with two mports on the same server platform,
the server must be configured as follows:
- "loopback" patch is NOT applied
- boot command line paramter is "rapidio.hdid=1"

NOTE: When running the paired test with two mports on the different server 
platforms, the server must be configured as follows:
- "loopback" patch is NOT applied
- boot command line paramter is "rapidio.hdid=1" if the enumerating server
has one mport.
- boot command line paramter is "rapidio.hdid=1,2" if the enumerating server
has two mports.

./kmsg -c 2 -M 1 -D 1 -S 0x1000 -T 1 -C 3335

gdb --args ./kmsg -c 2 -M 1 -D 1 -S 0x1000 -T 1 -C 3335 -d

============================================================================
Paired Test, Client

./kmsg -c 1 -M 0 -D 0 -S 0x1000 -T 1 -C 3335

gdb --args ./kmsg -c 1 -M 0 -D 0 -S 0x1000 -T 1 -C 3333 -d


=============================================================================
Dual Test

NOTE: When running the dual test with two mports on the same server platform,
the server must be configured as follows:
- "loopback" patch is NOT applied
- boot command line paramter is "rapidio.hdid=1"

NOTE: When running the dual test with two mports on the different server 
platforms, the server must be configured as follows:
- "loopback" patch is NOT applied
- boot command line paramter is "rapidio.hdid=1" if the enumerating server
has one mport.
- boot command line paramter is "rapidio.hdid=1,2" if the enumerating server
has two mports.

./kmsg -c 3 -S 0x1000 -T 1 -C 3333

gdb --args ./kmsg -c 3 -S 0x1000 -T 1 -C 3333

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
	1 - Client for paired Tsi721 on this platform
            *** Server must be started up before client!!! ***
	2 - Server for paired Tsi721 on this platform
            *** Server must be started up before client!!! ***
	3 - Dual Tsi721's in this platform.
            *** There must be two Tsi721's present to run this test ***

-k - Use kernel mode buffers

-v - Disable verification of transfers

-T - Number of times a test is repeated.  Default is 1.  Only loopback and
     dual mode tests can be repeated, paired tests may only be executed once.

-W - Tsi721(s) are connected through a sWitch

-t - Use separate threads to measure latency.  This measures latency from 
     the time the client starts to send a message, to the time 
     the server response message is received by the client.  Without this
     option, the blocking message send/receive calls cannot be measured 
     accurately in loopback and dual configurations.
