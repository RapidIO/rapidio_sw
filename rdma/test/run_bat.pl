#!/usr/bin/perl

# Script for running Batch Automated Tests (BATs).
#
# 1. On one computer run 'bat_server'
# 2. On a second computer run this script

#Open log file
open(my $fh, ">", "bat.log")
	or die "cannot open > bat.log $!";

system("sudo ./bat_client -d1 -c9 -ta -obat.log");
system("sudo ./bat_client -d1 -c9 -tb -obat.log");
system("sudo ./bat_client -d1 -c9 -tc -obat.log");
system("sudo ./bat_client -d1 -c9 -t1 -obat.log");
system("sudo ./bat_client -d1 -c9 -t2 -obat.log");
system("sudo ./bat_client -d1 -c9 -t3 -obat.log");
system("sudo ./bat_client -d1 -c9 -t4 -obat.log");
system("sudo ./bat_client -d1 -c9 -t5 -obat.log");
close($fh);
