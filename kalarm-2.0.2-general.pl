#!/usr/bin/perl -w
# Convert pre-2.0.2 General section settings.

use strict;

my @lines;
while (<>)
{
	chomp;
	if (/^(EmailFrom|EmailBccAddress)=\@ControlCenter$/) {
		push @lines, "$1=\@SystemSettings\n";
	}
}
if (@lines) {
	print "[General]\n";
	print @lines;
}
