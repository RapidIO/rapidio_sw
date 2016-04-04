#!/usr/bin/perl

# Perl script called during BAT runs.
#
use warnings;
use strict;

use Getopt::Std;
use Cwd 'abs_path';
use File::Spec;

# Obtain current directory so we can find 'bat_client' when called from a remote
# shell script
my ($volume, $directory, $file) = File::Spec->splitpath(__FILE__);

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
my $logfilename = $directory . "bat" . $channel . ".log";
open(my $fh, ">", $logfilename)
	or die "cannot open $logfilename!";

# Test case to run -- if specified
if (defined($options{t})) {
	print "sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t$options{t} -o" . $logfilename;
	system("sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t$options{t} -o" . $logfilename);
	close($fh);
	exit;
}

# List of tests
my @tests = (
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -ta -o" . $logfilename, #  0
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tb -o" . $logfilename, #  1
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tc -o" . $logfilename, #  2
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -td -o" . $logfilename, #  3
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -te -o" . $logfilename, #  4
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tf -o" . $logfilename, #  5
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tg -o" . $logfilename, #  6
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -th -o" . $logfilename, #  7
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -ti -o" . $logfilename, #  8
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tj -o" . $logfilename, #  9
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tk -o" . $logfilename, # 10
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tl -o" . $logfilename, # 11
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tm -o" . $logfilename, # 12
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tn -o" . $logfilename, # 13
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -to -o" . $logfilename, # 14
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tp -o" . $logfilename, # 15
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tr -o" . $logfilename, # 16
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tt -o" . $logfilename, # 17
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tu -o" . $logfilename, # 18
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t1 -o" . $logfilename, # 19
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t2 -o" . $logfilename, # 20
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t3 -o" . $logfilename, # 21
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t4 -o" . $logfilename, # 22
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t5 -o" . $logfilename, # 23
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t6 -o" . $logfilename, # 24
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t7 -o" . $logfilename, # 25
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t8 -o" . $logfilename, # 26
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -t9 -o" . $logfilename, # 27
	"sudo $directory/bat_client -d" . $destid. " -c" . $channel . " -n3 -tA -o" . $logfilename, # 28
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
print "******* Running Tests in Random Order *******\n";
for (my $i = 0; $i < $num_bat; $i++) {
	my $j = int(rand($num_bat));
	print "Running $tests[$j] at " . localtime() ."\n";
	system($tests[$j]);
}
close($fh);

