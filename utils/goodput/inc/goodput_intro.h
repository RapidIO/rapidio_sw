/*! \mainpage Goodput Introduction
 *
 * \section intro_sec Introduction
 * The goodput tool measures goodput (actual data transferred) and latency for 
 * the rapidio_sw/common/libmport direct i/o DMA, and Messaging interfaces.
 * It can also measure the goodput and latency for a separate Tsi721 
 * user mode driver tested on x86/x64 platforms.
 *
 * \section fast_start_sec Getting Started
 * \subsection compile_sec Compiling Goodput
 * To compile the goodput tool:
 * 1. In the "rapidio_sw" directory, type "make all"
 * 2. In the "rapidio_sw/utils/goodput" directory, type "make all"
 * \subsection exec_sec Running Goodput
 * The goodput tool must be run as root.  To execute goodput, type "sudo
 * ./goodput" while in the  "rapidio_sw/utils/goodput" directory.
 *
 * \section cli_secn Command Line Interpreter Overview
 *
 * \subsection Common CLI Commands
 * Goodput integrates the "rapidio_sw/common/libcli" command line interpreter
 * library.  This library has the following base commands:
 *
 * \subsubsection help_secn Help command
 * The libcli help command is "?".  Type "?" to get a list of commands, or "?
 * <command>" to get detailed help for a command.
 *
 * \subsubsection debug_secn Debug Command
 * Many commands have different levels of debug output.  The "debug" command
 * displays and optionally alters the debug output level.
 *
 * \subsubsection log_secn Log Command
 * The "log" command is used to capture the input and output of a
 * goodput CLI session.
 *
 * \subsubsection script_secn Script Command
 * Libcli supports accepting input from script files.  The "script" command
 * specifies a file name to be used as the source of commands for the CLI
 * session.  Script files may call other script files.  Every script file is
 * run in it's own CLI session, so environment changes (i.e. log file names)
 * in one script file do not affect the log file names in another.  See \ref
 * scrpath_secn for selecting a directory of script files.  The goodput
 * command comes with many script files in the
 * "rapidio_sw/utils/goodput/scripts"  directory and subdirectories.
 *
 * \subsubsection scrpath_secn Scrpath Command
 * Displays and optionally changes the directory path prepended to  script files
 * names.  Script files which do not begin with "/" or "\" have the prefix
 * prepended before the file is openned.
 *
 * \subsubsection echo_secn Echo Command
 * Displays a copy of the text following the command.  Useful for annotating
 * log files and scripts.
 *
 * \subsubsection quit_secn Quit Command
 * The quit command exits goodput cleanly, freeing up all libmport resources
 * that may be in use by goodput at the time.
 *
 * \section threads_secn Goodput Thread Management Overview
 * The goodput CLI is used to manage 8 worker threads.
 * CLI commands can be used to get each worker thread to
 * perform a measurement, as explained in \ref measurement_secn.  The
 * following commands are used to manage goodput threads:
 *
 * \subsection thread_secn Thread Command
 * The thread command is used to start a new thread.  Threads may be
 * required to run on a specific CPU, or may be allowed to run on any CPU.
 * Additionally, a thread may have a private copy of the master port handle, or
 * may use the shared copy of the master port handle.  A private copy of the
 * master port handle is used to request a DMA channel specific to that
 * thread.  For more information, see \ref dma_meas_secn.
 * A thread can be in one of three states: dead, running, and halted.  Dead
 * threads cannot process commands.  A halted thread can accept a new command.
 * A running thread is executing a measurement, and cannot accept a new
 * command until it stops running.  A running thread may be halted or killed.
 * For more information, see \ref halt_secn and \ref kill_secn.
 *
 * \subsection halt_secn Halt Command
 * Command used to request a thread to halt.  If the thread is currently
 * running a measurement, the thread wil halt and await the next command.
 *
 * \subsection kill_secn Kill Command
 * Command used to kill a thread, whatever its current state.  All resources
 * owned by the thread are release.  After a thread has been killed, the 
 * thread cannot process commands until it is restarted using the "thread"
 * command.  For more information, see \ref thread_secn.
 *
 * \subsection move_secn Move Command
 * If a thread is currently halted, it is possible to move the thread from one
 * cpu to another using the move command.  A moved thread retains all
 * allocated resources.
 *
 * \subsection wait_secn Wait Command
 * Wait until a thread has reached a particular state (dead, running, halted).
 * Useful in scripts to ensure a command has started running successfully, and
 * that threads have halted/died before issueing another command.
 *
 * \subsection stat_secn Status Command
 * The status command gives the current state of all threads.  Status has 
 * three variants: IBWIN status, messaging status, and general status.
 * General status is the fault.  IBWIN status gives information about Direct
 * I/O inbound window resources owned by the thread.  Messaging status gives
 * information about messaging resources owned by the thread. Generat status
 * gives information about the command that a thread is running/has run.
 *
 * \section measurement_secn Goodput Measurements
 * Goodput measures goodput and latency for Direct I/O, DMA, and messaging
 * transactions.  
 * 
 * Direct I/O and DMA both produce read and write transactions
 * that access an inbound window on a target device.  A thread on node X must
 * be commanded to allocate an inbound window z, and a thread on node Y must be
 * commanded to perform direct I/O or DMA transactions to Node X inbound
 * window z. 
 *
 * Messaging transactions
 * support socket style bind/listen/accept/connect semantics: A thread on node
 * X must be commanded to receive on socket z, and a thread on node Y must be
 * commanded to send to node X socket z, for messaging measurements to occur.
 *
 * The IBWIN command requests that a thread allocate an inbound window.  Once
 * the thread has finished inbound window allocation, the thread halts and can
 * accept another command.  The location of the inbound window may be
 * displayed using the "status i" command.  For more information, refer to
 * \ref status_secn.
 *
 * All commands which act as sources of transactions require a destination ID
 * for the receiver.  To determine the destination ID of the receiver, execute
 * the "mpdevs" command.
 *
 * Goodput (amount of data transferred ) measurements are displayed by the
 * "goodput" command.
 *
 * Latency measurements are displayed by the "lat" command.
 *
 * \subsection dio_meas_secn Direct I/O Measurement
 * Direct I/O read and write transactions are generally performed 
 * as processor reads and writes to device specific memory addresses, so the 
 * transaction sizes are restricted to 1/2/4 and 8 bytes.  Any other 
 * transaction size can be used to measure the overhead of the goodput 
 * infrastructure for measuring goodput/latency.
 * 
 * Direct I/O measurements require two threads: one on the source of the
 * transactions, and one on the target. 
 *
 * The target thread must have allocated an inbound window using the "IBWIN"
 * command.  
 *
 * The source thread must be commanded to access the inbound window at the
 * destination id of the target thread, and the address of the target thread.
 *
 * \subsubsection dio_thruput_secn Direct I/O Goodput Measurement
 *
 * The OBDIO command is used to measure goodput for Direct I/O transactions.
 * For example, to measure the goodput for 8 byte write accesses to an
 * inbound window 0x400000 bytes in size located at address 0x22f000000 found
 * on the device with destination ID of 9, using thead 3, type:
 *
 * OBDIO 3 9 22f000000 400000 8 1
 *
 * \subsubsection dio_lat_secn Direct I/O Latency Measurement
 *
 * Latency measurement for direct I/O read transactions can be performed by
 * the source node without assistance from th target node.  For example, to
 * measure the latency of 2 byte read accesses to an inbound window 0x200000
 * bytes in size located at address 0x22f000000 found on the device with
 * destination ID of 9, using thread 4, type:
 *
 * DIOTxLat 4 9, 22f000000 2 0
 *
 * Latency measurement for direct I/O write transactions requires a thread on
 * the target node to "loop back" the write performed by the source node.
 * Measuring write latency is a two stage process: Start the "loop back"
 * thread command running on the receiver using the DIORxLat command, then 
 * initiate transmission on the source thread with the DIOTxLat command.
 *
 * On the receiver, initiate the "loop back" for 4 byte writes from the 
 * source with destination ID of 7, being written to the receivers inbound
 * window at address 0x22f400000, using thread 6:
 *
 * DIORxLat 6 7 22f400000 4
 *
 * On the source, initiate the 4 byte writes to the receiver with destination
 * ID of 8 using thread 3:
 *
 * DIOTxLat 3 8 22f400000 4 1
 *
 * \subsection dma_meas_secn DMA Measurement
 * DMA read and write goodput measurements are performed as a sequence of
 * smaller transactions which add up to a total number of bytes.  Once the
 * total number of bytes specified has been transferred, the DMA goodput
 * statistics are updated.  For example, it is possible to transfer 4 MB of
 * data as a single DMA transaction, or as a sequence of 64 KB transactions.
 * The sequence of 64 KB transactions would require more DMA descriptors and
 * more processing, hence the goodput for smaller transactions is generally 
 * lower than for larger transactions.
 *
 * Similarly, DMA transactions may be performed using kernel buffers
 * (contiguous physical memory) or user mode buffers (discontiguous memory).
 * User mode buffers generally give lower goodput, since they are
 * discontiguous physical memory and so require more DMA
 * descriptors/transactions to effect a transfer.
 * 
 * DMA measurements require two threads: one on the source of the
 * transactions, and one on the target.
 *
 * The target thread must have allocated an inbound window using the "IBWIN"
 * command.  
 *
 * The source thread must be commanded to access the inbound window at the
 * destination id of the target thread, and the address of the target thread.
 *
 * \subsubsection dma_thruput_secn DMA Goodput Measurement
 *
 * The "dma" command is used to measure goodput for Direct I/O transactions.
 * For example, to measure the goodput for 4 MB write accesses to an
 * inbound window 0x400000 (4 MB) bytes in size located at address
 * 0x22f000000 found
 * on the device with destination ID of 9, using thead 3, type:
 *
 * dma 3 9 22f000000 400000 400000 1 1 0 0
 *
 * \subsubsection dma_lat_secn DMA Latency Measurement
 *
 * Latency measurement for DMA read transactions can be performed by
 * the source node without assistance from th target node.  For example, to
 * measure the latency of 0x10 byte read accesses to an inbound window 0x200000
 * bytes in size located at address 0x22f000000 found on the device with
 * destination ID of 9, using thread 4, use the dTxLat command as follows:
 *
 * dTxLat 4 9 22f000000 10 0 1 0
 *
 * Latency measurement for DMA write transactions requires a thread on
 * the target node to "loop back" the write performed by the source node.
 * Measuring write latency is a two stage process: Start the "loop back"
 * thread command running on the receiver using the dRxLat command, then 
 * initiate transmission on the source thread with the dTxLat command.
 *
 * On the receiver, initiate the "loop back" for 0x200000 byte writes from the 
 * source with destination ID of 7, being written to the receivers inbound
 * window at address 0x22f400000, using thread 6:
 *
 * dRxLat 6 7 22f400000 200000 
 *
 * On the source, initiate the 0x200000 byte writes to the receiver with
 * destination
 * ID of 8 using thread 3:
 *
 * dTxLat 3 8 22f400000 200000 1 1 0 
 *
 * \subsection msg_meas_secn Messaging Measurement
 *
 * Unlike Direct I/O and DMA measurements, messaging measurements always
 * require a running receiving thread to perform any measurements.
 *
 * The receiving thread for goodput is run using the "msgRx" command,
 * and the source  thread is run using the "msgTx" command.
 * 
 * The receiving thread for latency is run using the "mRxLat" command,
 * and the source thread is run using the "mTxLat" command.
 * 
 * Note that the minimum message size is 24 bytes, and the maximum is 4096
 * bytes. A message must always be a multiple of 8 bytes in size.
 * 
 * \subsubsection msg_thruput_secn Messaging Goodput Measurement
 *
 * First, use receive thread 3 on the node with destID 5 and socket number 1234
 * to receive all of the messages sent.  The msgRx command looks like the
 * following:
 *
 * msgRx 3 1234
 *
 * On the transmitting node, use the msgTx command to send 4096 byte messages 
 * using thread 7 to the node with the msgRx thread as follows:
 *
 * msgTx 7 5 1234 4096
 *
 * \subsubsection msg_thruput_secn Messaging Latency Measurement
 *
 * First, use receive thread 3 on the node with destID 5 and socket number 1234
 * to send back 2048 byte messages to the source.  The mRxLat command looks 
 * like the following:
 *
 * mRxLat 3 1234 2048
 *
 * On the transmitting node, use the mTxLat command to send 2048 byte messages 
 * using thread 7 to the node with the mRxLat thread as follows:
 *
 * msgTx 7 5 1234 2048
 */
