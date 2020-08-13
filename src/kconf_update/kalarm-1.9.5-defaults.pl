#!/usr/bin/perl -w
# Convert pre-1.9.5 Defaults section settings.

# SPDX-License-Identifier: GPL-2.0-or-later

use strict;

my $param;
my $cmdLogType;
my $soundType;
while (<>)
{
	chomp;
	# Convert KAlarm pre-1.9.5 entries
	if (/^DefSoundVolume=(.*)$/) {
		$param = ($1 < 0) ? -1 : ($1 > 1) ? 100 : $1 * 100;
		print "SoundVolume=$param\n";
		print "# DELETE DefSoundVolume\n";
	}
	elsif (/^DefCmdLogType=(.*)$/) {
		$cmdLogType = ($1 == 1) ? "File"
		            : ($1 == 2) ? "Terminal" : "Discard";
		print "# DELETE DefCmdLogType\n";
	}
	elsif (/^DefRecurPeriod=(.*)$/) {
		$param = ($1 == 1) ? "Login"
		       : ($1 == 2) ? "SubDaily"
		       : ($1 == 3) ? "Daily"
		       : ($1 == 4) ? "Weekly"
		       : ($1 == 5) ? "Monthly"
		       : ($1 == 6) ? "Yearly" : "None";
		print "RecurPeriod=$param\n";
		print "# DELETE DefRecurPeriod\n";
	}
	elsif (/^DefRemindUnits=(.*)$/) {
		$param = ($1 == 1) ? "Days"
		       : ($1 == 2) ? "Weeks" : "HoursMinutes";
		print "RemindUnits=$param\n";
		print "# DELETE DefRemindUnits\n";
	}
	elsif (/^DefSoundType=(.*)$/) {
		if (!$soundType) {
			$soundType = ($1 == 1) ? "Beep"
			           : ($1 == 2) ? "File"
			           : ($1 == 3) ? "Speak" : "None";
		}
		print "# DELETE DefSoundType\n";
	}

	# Convert KAlarm pre-1.4.6 entries
	elsif (/^DefSound=(.*)$/) {
		if ($1 ne "true") {
			$soundType = "None";
		}
		print "# DELETE DefSound\n";
	}

	# Convert KAlarm pre-1.3.0 entries
	elsif (/^DefCmdXterm=(.*)$/) {
		$cmdLogType = ($1 eq "true") ? "Terminal" : "Discard";
		print "# DELETE DefCmdXterm\n";
	}
}

if ($cmdLogType) {
	print "CmdLogType=$cmdLogType\n";
}
if ($soundType) {
	print "SoundType=$soundType\n";
}
