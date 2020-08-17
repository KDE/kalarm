#!/usr/bin/perl -w
# Convert pre-2.1.5 General section settings.

# SPDX-FileCopyrightText: 2009 David Jarvie <djarvie@kde.org>
# SPDX-License-Identifier: GPL-2.0-or-later

use strict;

while (<>)
{
	if (/^CmdXTerm=konsole/) {
		s/ -T / -p tabtitle=/;
		print "[General]\n";
		print $_;
	}
}
