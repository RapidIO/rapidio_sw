#!/usr/bin/perl

use warnings;
use strict;

use Getopt::Std;

# Script for running Batch Automated Tests (BATs).
#
# 1. On one computer run 'bat_server'
# 2. On a second computer run this script

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
# Open log file for storing PASS/FAIL results of tests
my $logfilename = "bat" . $channel . ".log";
open(my $fh, ">", $logfilename)
	or die "cannot open $logfilename!";

# List of tests
my @tests = (
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -ta -o" . $logfilename, # 0
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tb -o" . $logfilename, # 1
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tc -o" . $logfilename, # 2
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -tg -o" . $logfilename, # 3
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -th -o" . $logfilename, # 4
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -ti -o" . $logfilename, # 5
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t1 -o" . $logfilename, # 6
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t2 -o" . $logfilename, # 7
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t3 -o" . $logfilename, # 8
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t4 -o" . $logfilename, # 9
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -t5 -o" . $logfilename, # 10
);

my $num_bat = scalar@tests;
print "There are $num_bat tests to run\n";


# Run tests, in sequence
foreach my $bat(@tests) {
	print "Running $bat at " . localtime() ."\n";
	system($bat);
}

# Run tests, in a random order
for (my $i = 0; $i < $num_bat; $i++) {
	my $j = int(rand($num_bat));
	print "Running $tests[$j] at " . localtime() ."\n";
	system($tests[$j]);
}
close($fh);
