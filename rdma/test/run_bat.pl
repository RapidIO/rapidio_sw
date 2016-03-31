#!/usr/bin/perl

use warnings;
use strict;

use Getopt::Std;

# Script for running Batch Automated Tests (BATs) on JUST TWO COMPUTERS
#
# 1. On a first computer run THREE instances of 'bat_server' using successive channel numbers
#    (e.g. bat_server -c2224, bat_server -c2225, and bat_server -c2226)
#
# 2. On a second computer run this script specifying the FIRST of the THREE channel numbers
#    using the -c option
#    (e.g. run_bat.pl -c2224)

# Command-line options
my %options = ();
getopts("c:d:h:t:", \%options);

if (defined($options{h})) {
	print "run_bat.pl -h -d<destid> -c<channel\n";
	print "-h	Displays this help message\n";
	print "-d	Destination ID running bat_server\n";
	print "-c	Channel for test signalling with FIRST of THREE bat_servers\n";
	print "-t	Test case to run - if omitted run all\n";
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
	$destid = hex($options{d});
}
# Open log file for storing PASS/FAIL results of tests
my $logfilename = "bat" . $channel . ".log";
open(my $fh, ">", $logfilename)
	or die "cannot open $logfilename!";

# Test case to run -- if specified
if (defined($options{t})) {
	system("sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t$options{t} -o" . $logfilename);
	close($fh);
	exit;
}

# List of tests
my @tests = (
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -ta -o" . $logfilename, #  0
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tb -o" . $logfilename, #  1
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tc -o" . $logfilename, #  2
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -td -o" . $logfilename, #  3
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -te -o" . $logfilename, #  4
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tf -o" . $logfilename, #  5
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tg -o" . $logfilename, #  6
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -th -o" . $logfilename, #  7
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -ti -o" . $logfilename, #  8
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tj -o" . $logfilename, #  9
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tk -o" . $logfilename, # 10
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tl -o" . $logfilename, # 11
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tm -o" . $logfilename, # 12
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tn -o" . $logfilename, # 13
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -to -o" . $logfilename, # 14
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tp -o" . $logfilename, # 15
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tr -o" . $logfilename, # 16
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tt -o" . $logfilename, # 17
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tu -o" . $logfilename, # 18
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t1 -o" . $logfilename, # 19
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t2 -o" . $logfilename, # 20
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t3 -o" . $logfilename, # 21
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t4 -o" . $logfilename, # 22
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t5 -o" . $logfilename, # 23
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t6 -o" . $logfilename, # 24
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t7 -o" . $logfilename, # 25
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t8 -o" . $logfilename, # 26
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -t9 -o" . $logfilename, # 27
	"sudo ./bat_client -d" . $destid. " -c" . $channel . " -n3 -tA -o" . $logfilename, # 28
);

my $num_bat = scalar@tests;
print "There are $num_bat tests to run\n";


# Run tests, in sequence
print "******* Running Tests Sequentially *******\n";
foreach my $bat(@tests) {
	print "Running $bat at " . localtime() ."\n";
	system($bat);
}

# Run tests, in a random order
#print "******* Running Tests in Random Order *******\n";
#for (my $i = 0; $i < $num_bat; $i++) {
#	my $j = int(rand($num_bat));
#	print "Running $tests[$j] at " . localtime() ."\n";
#	system($tests[$j]);
#}
close($fh);

