#!/usr/bin/perl -w
# Convert KAlarm pre-1.2.1 General section settings.

# SPDX-FileCopyrightText: 2010 David Jarvie <djarvie@kde.org>
# SPDX-License-Identifier: GPL-2.0-or-later

use strict;

my $haveEmailFrom = 0;
my $haveEmailUseCtrlCentre = 0;
my $haveEmailBccUseCtrlCentre = 0;
my $emailUseCtrlCentre    = 1;    # default = true
my $emailBccUseCtrlCentre = 1;    # default = true
my $emailAddress;
my $emailBccAddress;

while (<>)
{
	chomp;
	if (/^EmailFrom=(.*)$/) {
		$haveEmailFrom = 1;
	}
	elsif (/^EmailUseControlCenter=(.*)$/) {
		$haveEmailUseCtrlCentre = 1;
		$emailUseCtrlCentre = ($1 eq "true");
	}
	elsif (/^EmailBccUseControlCenter=(.*)$/) {
		$haveEmailBccUseCtrlCentre = 1;
		$emailBccUseCtrlCentre = ($1 eq "true");
	}
	elsif (/^EmailAddress=(.*)$/) {
		$emailAddress = $1;
	}
	elsif (/^EmailBccAddress=(.*)$/) {
		$emailBccAddress = $1;
	}
}

if (!$haveEmailFrom && $haveEmailUseCtrlCentre)
{
	my $bccUseCC = $haveEmailBccUseCtrlCentre ? $emailBccUseCtrlCentre : $emailUseCtrlCentre;
	print "EmailFrom=" . ($emailUseCtrlCentre ? "\@ControlCenter" : $emailAddress) . "\n";
	print "EmailBccAddress=" . ($bccUseCC ? "\@ControlCenter" : $emailBccAddress) . "\n";
	print "# DELETE EmailAddress\n";
	if ($haveEmailUseCtrlCentre) {
		print "# DELETE EmailUseControlCenter\n";
	}
	if ($haveEmailBccUseCtrlCentre) {
		print "# DELETE EmailBccUseControlCenter\n";
	}
}
