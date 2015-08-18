#!/usr/bin/perl -w
# Convert pre-2.1.5 General section settings.

use strict;

while (<>)
{
	if (/^CmdXTerm=konsole/) {
		s/ -T / -p tabtitle=/;
		print "[General]\n";
		print $_;
	}
}
