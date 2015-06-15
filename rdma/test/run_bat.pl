#!/usr/bin/perl

use warnings;
use strict;

use Getopt::Std;

# Script for running Batch Automated Tests (BATs).
#
# 1. On one computer run 'bat_server'
# 2. On a second computer run this script

# Make sure 'rpcbind' is running
my @rpcbind = `ps -fewa | grep rpcbind`;
if (!scalar @rpcbind) {
	print "ERROR: 'rpcbind' is not running. Please start and try again\n";
	exit;
}

# Command-line options
my %options = ();
getopts("c:d:h", \%options);

if (defined($options{h})) {
	print "run_bat.pl -h -d<destid> -c<channel\n";
	print "-h	Displays this help message\n";
	print "-d	Destination ID running bat_server\n";
	print "-c	Channel for test signalling with bat_server\n";
	exit;
}

# Channel
my $channel;
if (!defined($options{c})) {
	print "ERROR: Channel not specified. Use -c<channel>\n";
	exit;
} else {
	$channel = $options{c};
}

# Destination ID
my $destid;
if (!defined($options{d})) {
	print "ERROR: Destination not specified. Use -d<destid>\n";
	exit;
} else {
	$destid = $options{d};
}

# List of tests
my @tests = (
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -ta -obat.log",	# 0
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tb -obat.log",	# 1
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tc -obat.log",	# 2
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tg -obat.log",	# 3
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -th -obat.log",	# 4
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -ti -obat.log",	# 5
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t1 -obat.log",	# 6
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t2 -obat.log",	# 7
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t3 -obat.log",	# 8
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t4 -obat.log",	# 9
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t5 -obat.log",	# 10
);

my $num_bat = scalar@tests;
print "There are $num_bat tests to run\n";

# Open log file for storing PASS/FAIL results of tests
open(my $fh, ">", "bat.log")
	or die "cannot open > bat.log $!";

# Run tests, in sequence
foreach my $bat(@tests) {
	print "Running $bat at " . localtime() ."\n";
	system($bat);
}

# Run tests, in a random order
for (my $i = 0; $i < $num_bat*10; $i++) {
	my $j = int(rand($num_bat));
	print "Running $tests[$j] at " . localtime() ."\n";
	system($tests[$j]);
}
close($fh);
