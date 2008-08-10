#!/usr/bin/perl -w
# Convert pre-2.0.2 General section settings.

use strict;

while (<>)
{
	chomp;
	if (/^(Email(From|BccAddress)=)ControlCenter$/) {
		print "$1=SystemSettings\n";
	}
}
