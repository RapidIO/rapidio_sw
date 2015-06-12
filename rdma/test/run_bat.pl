#!/usr/bin/perl

# Script for running Batch Automated Tests (BATs).
#
# 1. On one computer run 'bat_server'
# 2. On a second computer run this script

#Open log file
my @tests = (
	"sudo ./bat_client -d1 -c9 -ta -obat.log",	# 0
	"sudo ./bat_client -d1 -c9 -tb -obat.log",	# 1
	"sudo ./bat_client -d1 -c9 -tc -obat.log",	# 2
	"sudo ./bat_client -d1 -c9 -tg -obat.log",	# 3
	"sudo ./bat_client -d1 -c9 -th -obat.log",	# 4
	"sudo ./bat_client -d1 -c9 -ti -obat.log",	# 5
	"sudo ./bat_client -d1 -c9 -t1 -obat.log",	# 6
	"sudo ./bat_client -d1 -c9 -t2 -obat.log",	# 7
	"sudo ./bat_client -d1 -c9 -t3 -obat.log",	# 8
	"sudo ./bat_client -d1 -c9 -t4 -obat.log",	# 9
	"sudo ./bat_client -d1 -c9 -t5 -obat.log",	# 10
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
for (my $i = 0; $i < $num_bat; $i++) {
	my $j = int(rand($num_bat));
	print "Running $tests[$j] at " . localtime() ."\n";
	system($tests[$j]);
}
close($fh);
