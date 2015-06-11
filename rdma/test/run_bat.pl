#!/usr/bin/perl

# Script for running Batch Automated Tests (BATs).
#
# 1. On one computer run 'bat_server'
# 2. On a second computer run this script

#Open log file
open(my $fh, ">", "bat.log")
	or die "cannot open > bat.log $!";

print $fh, "Test case A:\n";
system("sudo ./bat_client -d1 -c9 -ta -obat.log");
print $fh, "Test case B:\n";
system("sudo ./bat_client -d1 -c9 -tb -obat.log");
#./bat_client -d1 -c9 -tb
#./bat_client -d1 -c9 -tc
#./bat_client -d1 -c9 -tg
#./bat_client -d1 -c9 -th
#./bat_client -d1 -c9 -ti
#./bat_client -d1 -c9 -t1
#./bat_client -d1 -c9 -t2
#./bat_client -d1 -c9 -t3
#./bat_client -d1 -c9 -t4
#./bat_client -d1 -c9 -t5
close($fh);
