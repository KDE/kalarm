#!/usr/bin/perl -w
# Convert KAlarm pre-1.2.1 General section settings.

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


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
