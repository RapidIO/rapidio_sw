#!/usr/bin/perl

open my $in, '<', 'rdmad_xdr.b';
open my $out, '>', 'rdmad_xdr.c';

while (<$in>) {
	if (/register/) {
	} else {
		print $out $_;
	}
}
close $in;
close $out;
