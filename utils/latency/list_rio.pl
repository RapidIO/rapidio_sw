#!/usr/bin/perl

use strict;
use warnings;

# Local Rapid I/O devices
my @rapidio_ports = `ls /sys/class/rapidio_port`;
if( !scalar @rapidio_ports ) {
    print "No RapidIO devices detected.\n";
    exit;
} 
print "\nLocal Rapid I/O devices:\n\n";
print "Master port\tDevice ID\n";
print "-----------\t---------\n";
my @local_destids;
foreach my $rapidio_port(sort @rapidio_ports) {
    chomp $rapidio_port;
    my $destid = `cat /sys/class/rapidio_port/$rapidio_port/port_destid`;
    chomp $destid;
    push( @local_destids, $destid );
    print $rapidio_port . "\t" . $destid . "\n";
}

# Remote Rapido I/O devices
my @sys_bus_rapidio_devices = `ls /sys/bus/rapidio/devices`;
if( !scalar @sys_bus_rapidio_devices ) {
    # No devices at all which means no enumeration was run
    print "\n\nThere are no reachable remote RapidIO devices.\n";
    exit;
} 

print "\n\nRemote Rapid I/O devices:\n\n";
print "Endpoint   \tDest ID\n";
print "-----------\t---------\n";
foreach my $rapidio_device(@sys_bus_rapidio_devices) {
    if( $rapidio_device =~ /\d{2}\:e\:\d{4}/ ) {
	chomp $rapidio_device;
	my $destid = `cat /sys/bus/rapidio/devices/$rapidio_device/destid`;
	chomp $destid;
	# Exclude if local device
	my $is_local = 0;
        foreach my $local_destid(@local_destids) { 
	    if( $destid eq $local_destid ) {
		$is_local = 1;
	    }	    
	}
        print "$rapidio_device\t$destid\n" if !$is_local;
    }
}
