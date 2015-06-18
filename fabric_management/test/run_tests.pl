#!/usr/bin/perl

use strict;
use warnings;

### cfgparse test ###
print "\nStarting Cfgparse test\n\n";

my @cfgs = <"./test/testcfgs/*">;

die "no test configs" if (scalar @cfgs == 0);

for my $cfg (@cfgs) {
	print "Testing $cfg\n";
	my $ret = system("./fmd -t -c$cfg > test/out");

	if ($ret) {
		die "fail: test $cfg";
	} else {
		print "pass\n";
	}
}
