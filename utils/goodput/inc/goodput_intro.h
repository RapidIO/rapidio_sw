/*! \mainpage Goodput Introduction
 *
 * \section intro_sec Introduction
 * The goodput tool measures goodput (actual data transferred) and latency for 
 * the rapidio_sw/common/libmport direct I/O, DMA, and Messaging interfaces.
 * It can also measure the goodput and latency for a separate Tsi721 
 * user mode driver tested on x86/x64 platforms <Under Development>.
 *
 * \section fast_start_sec Getting Started
 * \subsection compile_sec Compiling Goodput
 * To compile the goodput tool:
 * 1. In the "rapidio_sw" directory, type "make all"
 * 2. In the "rapidio_sw/utils/goodput" directory, type "make all"
 * \subsection script_sec Performance Measurement Scripts
 * The goodput command line interpreter (CLI) supports a rich set of
 * performance evaluation
 * capabilities.  To simplify and automate performance evaluation, 
 * it is possible to generate scripts which can then be executed by 
 * the goodput CLI.  For more information on script eneration,
 * refer to \ref script_gen_detail_secn.
 * For more information on script execution,
 * refer to \ref script_exec_detail_secn.
 *
 * \subsection exec_sec Running Goodput
 * There are two versions of goodput: goodput, and ugoodput.  
 * Goodput contains all functionality and commands to verify kernel
 * mode applications.  Ugoodput
 * has all the commands and functionality of goodput, and includes 
 * commands and functionality to support the Tsi721 user mode driver.
 * Currently the Tsi721 user mode drive can only be compiled on x86/x64
 * platforms, and is still under development.
 *
 * The goodput and ugoodput tools must be run as root.
 * To execute goodput, type "sudo ./goodput" while in the
 * "rapidio_sw/utils/goodput" directory.
 *
 * Similarly, to execute ugoodput, type "sudo ./ugoodput" while in the
 * "rapidio_sw/utils/goodput" directory.
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
 * NOTE: the '.' command is identical to the script command
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
 * three variants: IBAlloc status, messaging status, and general status.
 * General status is the fault.  IBAlloc status gives information about Direct
 * I/O inbound window resources owned by the thread.  Messaging status gives
 * information about messaging resources owned by the thread. Generat status
 * gives information about the command that a thread is running/has run.
 *
 * \section measurement_secn Goodput Measurements Overview
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
 * The IBAlloc command requests that a thread allocate an inbound window.  Once
 * the thread has finished inbound window allocation, the thread halts and can
 * accept another command.  The location of the inbound window may be
 * displayed using the "status i" command.  The IBDealloc command may be used
 * to deallocate a previously allocated window.  For more information,
 * refer to \ref stat_secn.
 *
 * All commands which act as sources of transactions require a destination ID
 * for the receiver.  To determine the destination ID of the receiver, execute
 * the "mpdevs" command.  Mpdevs gives output of the following format:
 *
 * Available 1 local mport(s):
 * +++ mport_id: 0 dest_id: 0
 * RIODP: riomp_mgmt_get_ep_list() has 1 entries
 *         1 Endpoints (dest_ID): 1
 *
 * In this case, the destination ID of the node is 0.
 *
 * Goodput (amount of data transferred ) measurements are displayed by the
 * "goodput" command.
 *
 * Latency measurements are displayed by the "lat" command.
 *
 * \subsection script_gen_detail_secn Goodput Script Generation Details
 *
 * The bash script goodput/scripts/performance/make_all_scripts.sh creates
 * a complete set of scripts which can be used to evaluate the performance
 * of a platform. Individual scripts are found in one of the directories listed
 * below. For more details of the other scripts generated, see
 * \ref script_exec_detail_secn.
 *
 * - dma_lat : DMA latency measurement
 * - dma_thru : DMA goodput measurement for a single thread
 * - pdma_thru : Parallel DMA goodput measurement, using multiple threads
 * - msg_lat : Messaging latency measurement
 * - msg_thru: Messaging throughput measurement
 * - obwin_thru: Direct I/O latency measurement with one or multiple threads
 * - obwin_lat: Direct I/O latency measurement
 *
 * Each directory contains a bash script named create_scripts.sh, and one
 * or more template files used by the bash script as the basis of the
 * scripts to be created.  Each create_scripts.sh file accepts parameters
 * that are used to replace keywords in the template file(s).
 *
 * The make_all_scripts.sh script accepts the following parameters:
 * - WAIT       : Time in seconds to wait before perf measurement
 * - DMA_TRANS  : DMA transaction type
 *                0 NW, 1 SW, 2 NW_R, 3 SW_R 4 NW_R_ALL
 * - DMA_SYNC   : 0 - blocking, 1 - async, 2 - fire and forget
 * - DID        : Device ID of target device for performance scripts
 * - IBA_ADDR   : Hex address of target window on DID
 * - TX_DID     : Device ID of this device
 * - TX_IBA_ADDR: Hex address of local target window (TX_DID)
 * - SOCKET_PFX : First 3 digits of 4 digit socket numbers
 * 
 * - Optional parameters, if not entered same as DMA_SYNC
 *   - DMA_SYNC2  : 0 - blocking, 1 - async, 2 - fire and forget
 *   - DMA_SYNC3  : 0 - blocking, 1 - async, 2 - fire and forget
 *
 * These parameters are then passed to the create_scripts.sh bash scripts
 * in each subdirectory, as appropriate.  It is also possible to run the
 * create_scripts.sh scripts on their own.  Each script will describe the
 * parameters it accepts if called without any parameters.
 *
 * The DID, IBA_ADDR, TX_DID, AND TX_IBA_ADDR parameters are unique for
 * each pair of nodes.  To determine the parameter values for nodes A and B,
 * perform the following:
 *
 * -# Start goodput on Node A and Node B
 * -# Execute "t 0 0 0" on Node A and Node B
 * -# Execute "IBA 0 400000" on Node A and Node B
 * -# Execute "st i" on Node A and Node B to display the RapidIO address
 * -# Execute "mpd" on Node A and Node B to display the RapidIO device ID
 *
 * On Node A, make_all_scripts.sh should be called with:
 * - DID        : Node B device ID
 * - IBA_ADDR   : Node B RapidIO Address
 * - TX_DID     : Node A device ID
 * - TX_IBA_ADDR: Node A RapidIO Address
 * 
 * On Node B, make_all_scripts.sh should be called with:
 * - DID        : Node A device ID
 * - IBA_ADDR   : Node A RapidIO Address
 * - TX_DID     : Node B device ID
 * - TX_IBA_ADDR: Node B RapidIO Address
 * 
 * \subsection script_exec_detail_secn Goodput Script Execution Details
 *
 * Top level scripts are generated by each of the create_scripts.sh bash
 * scripts.  The top level scripts run all of the
 * scripts in each of the directories. The top level scripts are found
 * in the scripts/performance directory, and are named according to the
 * directory and function of scripts that will be executed.
 *
 * All top level scripts can themselves be executed by the 
 * scripts/run_all_perf script.
 *
 * Note that some latency scripts (dma write latency,
 * Direct I/O write latency, and messaging latency) cannot be 
 * executed from a top level script because they require similar scripts
 * to be run on the target of the write and the source of the write.
 *
 * \subsection dio_meas_secn Direct I/O Measurement
 * Direct I/O read and write transactions are generally performed 
 * as processor reads and writes, so the 
 * transaction sizes are restricted to 1/2/4 and 8 bytes.  Any other 
 * transaction size can be used to measure the overhead of the goodput 
 * infrastructure for measuring goodput/latency.
 * 
 * Direct I/O measurements require two threads: one on the source of the
 * transactions, and one on the target. 
 *
 * The target thread must have allocated an inbound window using the "IBAlloc"
 * command.  
 *
 * The source thread must be commanded to access the inbound window at the
 * destination id of the target thread, and the address of the target thread.
 *
 * \subsubsection dio_thruput_scr_secn Direct I/O Goodput Measurement Scripts
 *
 * All direct IO read measurements can be performed by executing the
 * scripts/performance/obwin_thru_read top level script from the Goodput
 * command prompt.
 * The performance results are captured in the obwin_thru_read.log file,
 * found in the rapidio_sw/utils/goodput directory.
 *
 * All direct IO write measurements can be performed by executing the
 * scripts/performance/obwin_thru_write top level script from the Goodput
 * command prompt.
 * The performance results are captured in the obwin_thru_write.log file,
 * found in the rapidio_sw/utils/goodput directory.
 *
 * Direct I/O goodput measurement scripts are found in the
 * scripts/performance/obwin_thru directory.  The script name format is
 * oNdSZ.txt, where:
 *
 * N is the number of threads performaning the access, either 1 or 8
 * 
 * d is the direction, either R for read or W for write
 *
 * SZ is the size of the access, consisting of a number of bytes followed by
 * one of B for bytes, K for kilobytes, or M for megabytes.
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
 * For further examples, refer to the generated direct I/O measurement scripts.
 *
 * \subsubsection dio_lat_scr_secn Direct I/O Latency Measurement Scripts
 *
 * Scripts to perform direct I/O latency measurement are found in the
 * scripts/performance/obwin_lat directory.  All script names have the 
 * format olDsz, where:
 *
 * D is the direction, either R for read, W for write, or T for looping
 * back received write data.
 *
 * sz is the size of the access, consisting of a number of bytes followed by
 * one of B for bytes, K for kilobytes, or M for megabytes.
 *
 * To execute all of the read latency scripts:
 * 1. Execute the script scripts/performance/obwin_lat/olT8B.txt on the target.
 * 2. Execute the script  scripts/performance/obwin_lat_read command on the
 * source.
 * 
 * All direct I/O read latency measurements are captured in the
 * rapidio_sw/utils/goodput/obdio_read_lat.log file.
 *
 * To execute individual write latency measurements, perform the following
 * steps:
 * 1. Kill all workers on the source node with the "kill all" command
 * 2. Execute the scripts/performance/obwin_lat/olTnB.txt script on the target
 * 3. Execute the scripts/performance/obwin_lat/olWnB.txt script on the source
 * node which will measure latency.
 *
 * The latency measurement result will be shown in the source node CLI session.
 *
 * It is not possible to execute all direct I/O write latency measurements
 * from a top level script.  
 *
 * \subsubsection dio_lat_secn Direct I/O Latency Measurement Commands
 *
 * Latency measurement for direct I/O read transactions can be performed by
 * the source node without assistance from the target node.  For example, to
 * measure the latency of 2 byte read accesses to an inbound window 0x200000
 * bytes in size located at address 0x22f000000 found on the device with
 * destination ID of 9, using thread 4, type:
 *
 * DIOTxLat 4 9 22f000000 2 0
 *
 * Direct I/O write latency measurements transactions require a thread on
 * the target node to "loop back" the write performed by the source node.
 * To measure write latency, perform the following steps:
 * 1. Start a thread on the source node, which will be used to measure
 * write latency.
 * 2. Allcate an inbound window using the IBAlloc command for the thread
 * on the source node.
 * 3. Start a thread on the target node, which will be used to loop back
 * received write data.
 * 4. Allocate an inbound window using the IBAlloc command for the thread
 * on the source node.
 * 5. Execute the DIORxLat command on the target node.
 * 6. Execute the DIOTxLat command on the source node. 
 *
 * For example, on the target, initiate the "loop back" for 4 byte writes from
 * the * source with destination ID of 7, being written to the sources inbound
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
 *
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
 * The target thread must have allocated an inbound window using the "IBAlloc"
 * command.  
 *
 * The source thread must be commanded to access the inbound window at the
 * destination id of the target thread, and the address of the target thread.
 *
 * \subsubsection dma_thruput_scr_secn DMA Goodput Measurement Scripts
 *
 * DMA Goodput measurement scripts are found in two directories:
 * dma_thru and pdma_thru.  Scripts in dma_thru use a single thread to
 * send packets to a single DMA queue.  Scripts in pdma_thru use 8 threads
 * to send packets from multiple DMA queues.  Depending on the capabilities of
 * the processor and the endpoint, pdma_thru scripts may provide more
 * goodput than dma_thru.
 *
 * Pdma_thru measurements vary based on the number of DMA engines used. 
 * The number of DMA engines used is captured in the name of the pdma_thru
 * top level script:
 *
 * pdma_thru_pd1_read  : Single DMA engine, multiple threads, read
 * pdma_thru_pd1_write : Single DMA engine, multiple threads, write
 * pdma_thru_pdd_read  : Multiple DMA engines, single thread per engine, read
 * pdma_thru_pdd_write : Multiple DMA engines, single thread per engine, write
 * pdma_thru_pdm_read  : Mix of single and multiple threads per engine, read
 * pdma_thru_pdm_write : Mix of single and multiple threads per engine, write
 *
 * All DMA read measurements can be performed by executing the
 * scripts/performance/dma_thru_read or pdma_thru_XXX_read scripts 
 * from the Goodput command prompt. The performance results are
 * captured in the dma_thru_read.log and pdma_thru_XXX_read.log files,
 * found in the rapidio_sw/utils/goodput directory.
 *
 * All DMA write measurements can be performed by executing the
 * scripts/performance/dma_thru_write or pdma_thru_XXX_write scripts 
 * from the Goodput command prompt. The performance results are
 * captured in the dma_thru_write.log and dma_thru_XXX_write.log files,
 * found in the rapidio_sw/utils/goodput directory.
 *
 * DMA goodput measurement scripts are found in the
 * scripts/performance/dma_thru and pdma_thru directorise.
 * The script name format is dNtSZ.txt, where:
 *
 * N is the number of threads performaning the access, either 1 or 6
 * 
 * t is the type of access, either R for read or W for write
 *
 * SZ is the size of the access, consisting of a number of followed by
 * one of B for bytes, K for kilobytes, or M for megabytes.  For example,
 * the file d6R64K.txt indicate that this measures dma Read throughput for 6
 * threads using transfers of 64 kilobytes.
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
 * \subsubsection dma_lat_scr_secn DMA Latency Measurement Scripts
 *
 * Scripts to perform DMA latency measurement are found in the
 * scripts/performance/dma_lat directory.  All script names have the 
 * format dlDsz, where:
 *
 * D is the direction, either R for read, W for write, or T for looping
 * back received write data.
 *
 * sz is the size of the access, consisting of a number followed by
 * one of B for bytes, K for kilobytes, or M for megabytes.  For example,
 * file dlT2M.txt is the script to run on the target for 2 megabyte DMA
 * transfers.
 *
 * To execute all of the read latency scripts:
 * 1. Execute the script scripts/performance/dma_lat/dlT4M.txt on the target.
 * 2. Execute the script  scripts/performance/dma_lat_read command on the
 * source.
 * 
 * All DMA read latency measurements are captured in the
 * rapidio_sw/utils/goodput/dma_read_lat.log file.
 *
 * To execute individual write latency measurements, perform the following
 * steps:
 * 1. Kill all workers on the source node with the "kill all" command
 * 2. Execute the scripts/performance/obwin_lat/dlTnB.txt script on the target
 * 3. Execute the scripts/performance/obwin_lat/dlWnB.txt script on the source
 * node which will measure latency.
 *
 * The latency measurement result will be shown in the source node CLI session.
 *
 * It is not possible to execute all DMA write latency measurements
 * from a single script.  
 *
 * \subsubsection dma_lat_secn DMA Latency Measurement Commands
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
 * \subsubsection msg_thruput_scr_secn Messaging Goodput Measurement Scripts
 *
 * Messaging goodput measurement scripts are found in the
 * scripts/performance/msg_thru directory.  To execute all messaging
 * performance scripts, perform the following steps:
 *
 * 1. Execute the scripts/performance/msg_thru/m_rx.txt script on the
 * target node.
 * 2. Execute the scripts/performance/msg_thru_tx.txt script on the
 * source node.
 *
 * All messaging throughput latency measurements are captured in the
 * rapidio_sw/utils/goodput/msg_thru_tx.log file.
 *
 * \subsubsection msg_thruput_secn Messaging Goodput Measurement Commands
 *
 * First, use receive thread 3 on the node with destID 5 and socket number 1234
 * following:
 *
 * msgRx 3 1234
 *
 * On the transmitting node, use the msgTx command to send 4096 byte messages 
 * using thread 7 to the node with the msgRx thread as follows:
 *
 * msgTx 7 5 1234 4096
 *
 * \subsubsection msg_thruput_scr_secn Messaging Latency Measurement Scripts
 *
 * Messaging latency measurement scripts are found in the
 * scripts/performance/msg_lat directory.  To measure latency for all
 * message sizes, perform the following steps:
 *
 * 1. Perform the scripts/performance/msg_lat/rx_
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
