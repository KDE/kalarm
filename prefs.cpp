/*
 *  prefs.cpp  -  program preferences
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "kalarm.h"

#include <qobjectlist.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qwhatsthis.h>

#include <kdialog.h>
#include <kglobal.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kapplication.h>

#include "fontcolour.h"
#include "prefsettings.h"
#include "prefs.moc"


PrefsBase::PrefsBase(QWidget* parent)
	: KTabCtl(parent)
{
}

QSize PrefsBase::sizeHintForWidget(QWidget* widget)
{
	// The size is computed by adding the sizeHint().height() of all
	// widget children and taking the width of the widest child and adding
	// layout()->margin() and layout()->spacing()

	QSize size;
	int numChild = 0;
	QObjectList* list = (QObjectList*)(widget->children());

	for (uint i = 0;  i < list->count();  ++i)
	{
		QObject* obj = list->at(i);
		if (obj->isWidgetType())
		{
			++numChild;
			QSize s = static_cast<QWidget*>(obj)->sizeHint();
			if (s.isEmpty())
				s = QSize(50, 100); // Default size
			size.setHeight(size.height() + s.height());
			if (s.width() > size.width())
				size.setWidth(s.width());
		}
	}

	if (numChild > 0)
	{
		size.setHeight(size.height() + widget->layout()->spacing()*(numChild-1));
		size += QSize(widget->layout()->margin()*2, widget->layout()->margin()*2 + 1);
	}
	else
		size = QSize(1, 1);

	return size;
}

void PrefsBase::setSettings(Settings* setts)
{
	mSettings = setts;
	restore();
}

void PrefsBase::apply(bool syncToDisc)
{
	mSettings->saveSettings(syncToDisc);
	mSettings->emitSettingsChanged();
}



MiscPrefs::MiscPrefs(QWidget* parent)
	: PrefsBase(parent)
{
	QWidget* page = new QWidget(this );
	QVBoxLayout* topLayout = new QVBoxLayout(page, 0, KDialog::spacingHint());
	topLayout->setMargin(KDialog::marginHint());

	QGroupBox* group = new QGroupBox(i18n("Run mode"), page, "modeGroup");
	topLayout->addWidget(group);
	QVBoxLayout* layout = new QVBoxLayout(group, KDialog::spacingHint(), 0);
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	QGridLayout* grid = new QGridLayout(group, 5, 2, KDialog::spacingHint());
	layout->addLayout(grid);
	grid->addRowSpacing(0, fontMetrics().lineSpacing()*2);
	grid->addRowSpacing(3, fontMetrics().lineSpacing()*2);
	grid->setColStretch(0, 0);
	grid->setColStretch(1, 2);
	grid->addColSpacing(0, 3*KDialog::spacingHint());
	// To have better control over the button layout, don't use a QButtonGroup

	// Run-in-system-tray radio button has an ID of 0
	mRunInSystemTray = new QRadioButton(i18n("Run continuously in system tray"), group, "runTray");
	mRunInSystemTray->setFixedSize(mRunInSystemTray->sizeHint());
	connect(mRunInSystemTray, SIGNAL(toggled(bool)), this, SLOT(slotRunInTrayToggled(bool)));
	QWhatsThis::add(mRunInSystemTray,
	      i18n("Check to run %1 continuously in the KDE system tray.\n\n"
	           "Notes:\n"
	           "1. With this option selected, closing the system tray icon will quit %2.\n"
	           "2. You do not need to select this option in order for alarms to be displayed, since alarm monitoring is done by the alarm daemon. Running in the system tray simply provides easy access and a status indication.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mRunInSystemTray, 0, 0, 0, 1, AlignLeft);

	mAutostartTrayIcon1 = new QCheckBox(i18n("Autostart at login"), page, "autoTray");
	QWhatsThis::add(mAutostartTrayIcon1,
	      i18n("Check to run %1 whenever you start KDE.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mAutostartTrayIcon1, 1, 1, AlignLeft);

	mDisableAlarmsIfStopped = new QCheckBox(i18n("Disable alarms while not running"), page, "disableAl");
	QWhatsThis::add(mDisableAlarmsIfStopped,
	      i18n("Check to disable alarms whenever %1 is not running. Alarms will only appear while the system tray icon is visible.").arg(kapp->aboutData()->programName()));
	grid->addWidget(mDisableAlarmsIfStopped, 2, 1, AlignLeft);

	// Run-in-system-tray radio button has an ID of 0
	mRunOnDemand = new QRadioButton(i18n("Run only on demand"), group, "runDemand");
	mRunOnDemand->setFixedSize(mRunOnDemand->sizeHint());
	connect(mRunOnDemand, SIGNAL(toggled(bool)), this, SLOT(slotRunOnDemandToggled(bool)));
	QWhatsThis::add(mRunOnDemand,
	      i18n("Check to run %1 only when required.\n\n"
	           "Notes:\n"
	           "1. Alarms are displayed even when %2 is not running, since alarm monitoring is done by the alarm daemon.\n"
	           "2. With this option selected, the system tray icon can be displayed or hidden independently of %3.")
	           .arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()).arg(kapp->aboutData()->programName()));
	grid->addMultiCellWidget(mRunOnDemand, 3, 3, 0, 1, AlignLeft);

	mAutostartTrayIcon2 = new QCheckBox(i18n("Autostart system tray icon at login"), page, "autoRun");
	QWhatsThis::add(mAutostartTrayIcon2,
	      i18n("Check to display the system tray icon whenever you start KDE."));
	grid->addWidget(mAutostartTrayIcon2, 4, 1, AlignLeft);

	grid = new QGridLayout(page, 1, 2, KDialog::spacingHint());
	topLayout->addLayout(grid);
	QLabel* lbl = new QLabel(i18n("System tray icon update interval [seconds]"), page);
	lbl->setFixedSize(lbl->sizeHint());
	grid->addWidget(lbl, 0, 0, AlignLeft);
	mDaemonTrayCheckInterval = new QSpinBox(1, 9999, 1, page, "daemonCheck");
	grid->addWidget(mDaemonTrayCheckInterval, 0, 1, AlignLeft);
	QWhatsThis::add(mDaemonTrayCheckInterval,
	      i18n("How often to update the system tray icon to indicate whether or not the Alarm Daemon is running."));

	topLayout->addStretch(1);
	page->setMinimumSize(sizeHintForWidget(page));

	addTab(page, i18n("&Miscellaneous"));
}

void MiscPrefs::restore()
{
	bool systray = mSettings->mRunInSystemTray;
	mRunInSystemTray->setChecked(!systray);
	mRunOnDemand->setChecked(systray);      // toggle to ensure things are enabled/disabled correctly
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(mSettings->mDisableAlarmsIfStopped);
	mAutostartTrayIcon1->setChecked(mSettings->mAutostartTrayIcon);
	mAutostartTrayIcon2->setChecked(mSettings->mAutostartTrayIcon);
	mDaemonTrayCheckInterval->setValue(mSettings->mDaemonTrayCheckInterval);
}

void MiscPrefs::apply(bool syncToDisc)
{
	bool systray = mRunInSystemTray->isChecked();
	mSettings->mRunInSystemTray         = systray;
	mSettings->mDisableAlarmsIfStopped  = mDisableAlarmsIfStopped->isChecked();
	mSettings->mAutostartTrayIcon       = systray ? mAutostartTrayIcon1->isChecked() : mAutostartTrayIcon2->isChecked();
	mSettings->mDaemonTrayCheckInterval = mDaemonTrayCheckInterval->value();
	PrefsBase::apply(syncToDisc);
}

void MiscPrefs::setDefaults()
{
	bool systray = Settings::default_runInSystemTray;
	mRunInSystemTray->setChecked(!systray);
	mRunOnDemand->setChecked(systray);      // toggle to ensure things are enabled/disabled correctly
	mRunOnDemand->setChecked(!systray);
	mDisableAlarmsIfStopped->setChecked(Settings::default_disableAlarmsIfStopped);
	mAutostartTrayIcon1->setChecked(Settings::default_autostartTrayIcon);
	mAutostartTrayIcon2->setChecked(Settings::default_autostartTrayIcon);
	mDaemonTrayCheckInterval->setValue(Settings::default_daemonTrayCheckInterval);
}

void MiscPrefs::slotRunInTrayToggled(bool on)
{
	if (on  &&  mRunOnDemand->isOn()
	||  !on  &&  !mRunOnDemand->isOn())
		mRunOnDemand->setChecked(!on);
	mAutostartTrayIcon1->setEnabled(on);
	mDisableAlarmsIfStopped->setEnabled(on);
}

void MiscPrefs::slotRunOnDemandToggled(bool on)
{
	if (on  &&  mRunInSystemTray->isOn()
	||  !on  &&  !mRunInSystemTray->isOn())
		mRunInSystemTray->setChecked(!on);
	mAutostartTrayIcon2->setEnabled(on);
}


AppearancePrefs::AppearancePrefs(QWidget* parent)
	: PrefsBase(parent)
{
	QWidget* page = new QWidget(this );
	QVBoxLayout* layout = new QVBoxLayout(page, 0, KDialog::spacingHint());
	layout->setMargin(KDialog::marginHint());
	mFontChooser = new FontColourChooser(page, 0L, false, QStringList(), true, i18n("Font and Color"), false);
	layout->addWidget(mFontChooser);

	layout->addStretch(1);
	page->setMinimumSize(sizeHintForWidget(page));

	addTab(page, i18n("Message &Appearance"));
}

void AppearancePrefs::restore()
{
	mFontChooser->setBgColour(mSettings->mDefaultBgColour);
	mFontChooser->setFont(mSettings->mMessageFont);
}

void AppearancePrefs::apply(bool syncToDisc)
{
	mSettings->mDefaultBgColour = mFontChooser->bgColour();
	mSettings->mMessageFont     = mFontChooser->font();
	PrefsBase::apply(syncToDisc);
}

void AppearancePrefs::setDefaults()
{
	mFontChooser->setBgColour(Settings::default_defaultBgColour);
	mFontChooser->setFont(Settings::default_messageFont);
}
