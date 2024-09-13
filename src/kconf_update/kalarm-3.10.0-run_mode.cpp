/* SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <KConfigGroup>
#include <KSharedConfig>
using namespace Qt::StringLiterals;

int main()
{
    // Read old AutoStart and NoAutoStart values
    auto kalarmrc = KSharedConfig::openConfig("kalarmrc"_L1, KConfig::SimpleConfig);
    auto generalGroup = kalarmrc->group(QStringLiteral("General"));
    bool autoStart   = generalGroup.readEntry("AutoStart", false);
    const bool noAutoStart = generalGroup.readEntry("NoAutoStart", false);

    // Write new RunMode entry, update AutoStart to correspond with it,
    // and remove old NoAutoStart entry.
    const QString runMode = noAutoStart ? QStringLiteral("Manual") : autoStart ? QStringLiteral("Auto") : QStringLiteral("None");
    generalGroup.writeEntry("RunMode", runMode);
    if (noAutoStart && autoStart)
	    generalGroup.writeEntry("AutoStart", false);
    generalGroup.deleteEntry("NoAutoStart");
    return kalarmrc->sync() ? 0 : 1;
}
