#!/usr/bin/perl

use warnings;
use strict;

use Getopt::Std;

# Clear the screen
system("clear");

# Indicate which type of test we are running!
print "*** User-mode DMA latency test ***\n";

my %options = ();
getopts("d:ghc:m:l:vw",\%options);

# The script must be run as 'root'
my $user = `echo \$USER`;
chomp $user;
if( $user ne 'root' ) {
    print "\n" . $user . " is not 'root'\n";
    print "Warning: Attempting to run test when not 'root' can fail.\n";
}

# Make sure 'rio_mport_cdev' is loaded
my @lsmod = `lsmod | grep rio_mport_cdev`;
if( !(scalar @lsmod) ) {
    print "rio_mport_cdev is not loaded. Please load and try again\n";
    exit;
}

# Check for help
if( defined($options{h}) ) {
    print "udma.pl -hgvw -c<configuration> -d<destid> -l<data length> -m<mport>\n";
    print "-c    Test configuration. 0=Loopback, 1=Dual-card, 2=Master, 3=Slave, 4=Test-board\n";
    print "-h    Displays this help message\n";
    print "-w    Test is run via a switch\n";
    print "-d    Destination ID\n";
    print "-l    Length of data to transfer in bytes. Use 'K' and 'M' for kilobytes/Megabytes\n";
    print "-m    Source master port (mport) number\n";
    print "-g    Run in 'gdb'\n";
    print "-v    Verbose mode\n";
    exit;
}

# Get the mode, and validate it
my $mode;
if( !defined($options{c}) ) {
    print "No configuration specified, using loopback configuration.\n";
    $mode = 0;
} else {
    $mode = $options{c};
    if( $mode < 0 || $mode > 4 ) {
        print "Sorry, valid values for configuration are: 0=Loopback, 1=Dual-card, ";
        print "2=Master 3=Slave 4=Test-board\n"; 
        exit;
    } 
} 

# Get the switch status, and validate against demo mode
my $has_switch = 0;
$has_switch = 1 if defined($options{w});

if( !$has_switch && $mode != 0 ) {
    while(1) {
        print "WARNING: Configuration $mode specified but -w not specified. Continue? (y/n)";
        my $choice = <STDIN>;
        if( $choice =~ /y|Y/ ) { goto CONT; }
        if( $choice =~ /n|N/ ) { exit; }
    }
}
CONT:

# Testboard mode requires enumeration on the local machine
my @sys_bus_rapidio_devices = `ls /sys/bus/rapidio/devices`;
if( $mode == 4 ) {
    if( !scalar @sys_bus_rapidio_devices ) {
        # No devices at all which means no enumeration was run
        print "You need to run enumeration since you've selected configuration $mode\n";
        exit;
    } else {
        # And just to be safe, ensure there is a switch present as well
        my $found_switch = 0;
        foreach my $rapidio_device(@sys_bus_rapidio_devices) {
            if( $rapidio_device =~ /\d{2}\:s\:\d{4}/ ) {
                $found_switch = 1;
            }
        } 
        if( !$found_switch ) {
            print "Switch not properly enumerated on this device. Aborting!\n";
            exit;
        }
    }
}

# Find out how many RapidIO devices are on the system
my $num_devices= 0;
my $rio_dir = '/sys/class/rapidio_port';
my @rio_devices;
opendir(my $dh, $rio_dir) or die "opendir($rio_dir): $!";
while (my $de = readdir($dh)) {
    next if $de =~ /^\./;   # Skip . and ..
    push( @rio_devices, $de );    
    $num_devices++;
}
closedir($dh);

# Validate mode against number of devices against 
if( ($mode == 1) && ($num_devices < 2) ) {
    print "Sorry, you cannot use a dual-card configuration unless I can detect 2 cards in your system.\n";
    exit;
}

# Get the 'mport' and validate against the number of local RapidIO devices
# If not specified then default to 0, except for Master and Slave where
# destination ID is specified
my $mport = 0;
if( defined($options{m}) ) {
    $mport = $options{m};
    if( $mport > ($num_devices - 1) ) {
        print "Invalid mport $mport. Valid options are only 0 through " . ($num_devices - 1) . "\n";
        exit;
    }
} 

# Get the destination ID then do some checks against the modes
my $destid = 0xFFFF; # Inint to an invalid destid
if( $mode == 1 || $mode == 2 || $mode == 3 ) {
    # If destination ID is defined, use it
    if( defined($options{d}) ) {
        $destid = $options{d};
    } else {
        print "ERROR: Using configuration $mode, but destination ID not specified.\n";
        exit;
    } 
} else {
    if( defined($options{d}) ) {
        print "Ignoring destination ID for configuration $mode.\n" if defined($options{v});
    }
}

# Get the data length
my $data_length;
if( !defined($options{l}) ) {
    print "Data length is missing, use '-l <length>', exiting.\n";
    exit;
} else {
    $data_length = $options{l};
    # Parse the data length interpreting 'K' and 'M'
    if( $data_length =~ /\d+[Kk]/ ) {
        chop $data_length;
        if( $data_length > 2048 ) {
            print $data_length . "K is too large. Maximum size is 2048K. Please try again\n";
            exit;
        }
        $data_length *= 1024;
    } elsif ( $data_length =~ /\d+[Mm]/ ) {
        chop $data_length;
        if( $data_length > 2 ) {
            print $data_length . "M is too large. Maximum size is 2M. Please try again\n";
            exit;
        }
        $data_length = $data_length * 1024 * 1024;
    }
}

# Prepare a command string
my $cmd_str = "./udma $mode $has_switch $destid $mport $data_length";

# Append number of devices to command string
$cmd_str .= " " . $num_devices;

# For each device, append path to command string, as well as
# obtain BAR0 size and append it to command string
foreach my $device(sort @rio_devices) {
    my $device_resources = "$rio_dir" . "/" . $device . "/device";
    $cmd_str .= " " . $device_resources;

    my @resource = `cat $device_resources/resource`;
    my @bar0 = split( ' ', $resource[0]);
    my $bar0_size = hex($bar0[1]) - hex($bar0[0]) + 1;
    $cmd_str = $cmd_str . " " . $bar0_size;
}

# Echo command string if verbose mode is enabled
print "$cmd_str\n" if( defined($options{v}));

# If -g is specified, run the program in 'gdb'
if( defined($options{g}) ) {
    $cmd_str = "gdb --args " . $cmd_str;
} 

system( $cmd_str );
