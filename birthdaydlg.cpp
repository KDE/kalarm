/*
 *  birthdaydlg.cpp  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qwhatsthis.h>

#include <klistview.h>
#include <klocale.h>
#include <kconfig.h>
#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "editdlg.h"
#include "soundpicker.h"
#include "spinbox.h"
#include "checkbox.h"
#include "colourcombo.h"
#include "prefsettings.h"
#include "birthdaydlg.moc"

using namespace KABC;


class AddresseeItem : public QListViewItem
{
  public:
    enum columns { Name = 0, Birthday = 1 };
    AddresseeItem(QListView *parent, const Addressee &addressee);
    Addressee       addressee() const { return mAddressee; }
    virtual QString key(int column, bool ascending) const;
  private:
    Addressee mAddressee;
    QString   mBirthdayOrder;
};



BirthdayDlg::BirthdayDlg(QWidget* parent)
	: KDialogBase(KDialogBase::Plain, i18n("Import Birthdays from KAddressBook"), Ok|Cancel, Ok, parent)
{
	QWidget* topWidget = plainPage();
	QBoxLayout* topLayout = new QVBoxLayout(topWidget);
	topLayout->setSpacing(spacingHint());

	QGroupBox* group = new QGroupBox(1, Qt::Horizontal, i18n("Select Birthdays"), topWidget);
	topLayout->addWidget(group);
	mAddresseeList = new KListView(group);
	mAddresseeList->setMultiSelection(true);
	mAddresseeList->setAllColumnsShowFocus(true);
#if KDE_VERSION >= 290
	mAddresseeList->setFullWidth(true);
#endif
	mAddresseeList->addColumn(i18n("Name"));
	mAddresseeList->addColumn(i18n("Birthday"));
	connect(mAddresseeList, SIGNAL(selectionChanged()), SLOT(slotSelectionChanged()));
	QWhatsThis::add(mAddresseeList,
	      i18n("Select birthdays to set alarms for.\n"
	           "This list shows all birthdays in KAddressBook except those for which alarms already exist."));

	group = new QGroupBox(i18n("Alarm Configuration"), topWidget);
	topLayout->addWidget(group);
	QBoxLayout* groupLayout = new QVBoxLayout(group, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	groupLayout->addSpacing(fontMetrics().lineSpacing()/2);

	// Prefix and suffix to the name in the alarm text
	// Get default prefix and suffix texts from config file
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));
	QString prefix = config->readEntry(QString::fromLatin1("BirthdayPrefix"), i18n("Birthday: "));
	QString suffix = config->readEntry(QString::fromLatin1("BirthdaySuffix"));

	QGroupBox* textGroup = new QGroupBox(2, Qt::Horizontal, i18n("Alarm text"), group);
	groupLayout->addWidget(textGroup);
	QLabel* label = new QLabel(i18n("&Prefix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	mPrefix = new QLineEdit(prefix, textGroup);
	mPrefix->setMinimumSize(mPrefix->sizeHint());
	label->setBuddy(mPrefix);
	QWhatsThis::add(mPrefix,
	      i18n("Enter text to appear before the person's name in the alarm message, "
	           "including any necessary trailing spaces."));

	label = new QLabel(i18n("S&uffix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	mSuffix = new QLineEdit(suffix, textGroup);
	mSuffix->setMinimumSize(mSuffix->sizeHint());
	label->setBuddy(mSuffix);
	QWhatsThis::add(mSuffix,
	      i18n("Enter text to appear after the person's name in the alarm message, "
	           "including any necessary leading spaces."));

	// How much to advance warning to give
// Disable until a general warning mechanism is implemented in KAlarm
	QBoxLayout* layout = new QHBoxLayout(groupLayout);
	label = new QLabel(i18n("Set &alarm"), group);
	label->setFixedSize(label->sizeHint());
label->setEnabled(false);
	layout->addWidget(label);
	mAdvance = new SpinBox(0, 364, 1, group);
	mAdvance->setValue(364);
	mAdvance->setFixedSize(mAdvance->sizeHint());
	mAdvance->setValue(0);
mAdvance->setEnabled(false);
	label->setBuddy(mAdvance);
	QWhatsThis::add(mAdvance,
	      i18n("Enter the number of days before each birthday to set the alarm."));
	layout->addWidget(mAdvance);
	label = new QLabel(i18n("days before"), group);
	label->setFixedSize(label->sizeHint());
label->setEnabled(false);
	layout->addWidget(label);
	layout->addStretch();

	// Sound checkbox and file selector
	QGridLayout* grid = new QGridLayout(groupLayout, 2, 2, KDialog::spacingHint());
	grid->setColStretch(1, 1);
	mSoundPicker = new SoundPicker(false, group);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	grid->addWidget(mSoundPicker, 0, 0, Qt::AlignLeft);

#ifdef SELECT_FONT
	// Font and colour choice drop-down list
#endif

	// Colour choice drop-down list
	mBgColourChoose = EditAlarmDlg::createBgColourChooser(false, group);
	grid->addWidget(mBgColourChoose, 0, 1, Qt::AlignRight);

	// Acknowledgement confirmation required - default = no confirmation
	mConfirmAck = EditAlarmDlg::createConfirmAckCheckbox(false, group);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	grid->addWidget(mConfirmAck, 1, 0, Qt::AlignLeft);

	// Late display checkbox - default = allow late display
	mLateCancel = EditAlarmDlg::createLateCancelCheckbox(false, group);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	grid->addWidget(mLateCancel, 1, 1, Qt::AlignRight);

	// Set the values to their defaults
	Settings* settings = theApp()->settings();
#ifdef SELECT_FONT
#endif
	mBgColourChoose->setColour(settings->defaultBgColour());     // set colour before setting alarm type buttons
	mLateCancel->setChecked(settings->defaultLateCancel());
	mConfirmAck->setChecked(settings->defaultConfirmAck());
	mSoundPicker->setChecked(settings->defaultBeep());

	// Compile a list of all pending alarm messages which look like birthdays
	QStringList messageList;
	KAlarmEvent event;
	QPtrList<Event> events = theApp()->getCalendar().events();
	for (Event* kcalEvent = events.first();  kcalEvent;  kcalEvent = events.next())
	{
		event.set(*kcalEvent);
		if (event.action() == KAlarmEvent::MESSAGE
		&&  event.recurs() == KAlarmEvent::ANNUAL_DATE
		&&  (prefix.isEmpty()  ||  event.message().startsWith(prefix)))
			messageList.append(event.message());
	}

	// Fetch all birthdays from the address book
	mAddressBook = StdAddressBook::self();
	for (AddressBook::Iterator it = mAddressBook->begin(); it != mAddressBook->end(); ++it)
	{
		if ((*it).birthday().isValid())
		{
			// Create a list entry for this birthday
			AddresseeItem* item = new AddresseeItem(mAddresseeList, (*it));
			// Remove the list entry if this birthday already has an alarm
			QString text = prefix + item->text(AddresseeItem::Name) + suffix;
			if (messageList.find(text) != messageList.end())
				delete item;
		}
	}

	enableButtonOK(false);   // nothing is currently selected
}

BirthdayDlg::~BirthdayDlg()
{
}

/******************************************************************************
* Return a list of events for birthdays chosen.
*/
QValueList<KAlarmEvent> BirthdayDlg::events() const
{
	QValueList<KAlarmEvent> list;
	QDate today  = QDate::currentDate();
	int thisYear = today.year();
	for (QListViewItem* item = mAddresseeList->firstChild();  item;  item = item->nextSibling())
	{
		if (mAddresseeList->isSelected(item))
		{
			AddresseeItem* aItem = dynamic_cast<AddresseeItem*>(item);
			if (aItem)
			{
				QDate date = aItem->addressee().birthday().date();
				date.setYMD(thisYear, date.month(), date.day());
				if (date <= today)
					date.setYMD(thisYear + 1, date.month(), date.day());
				KAlarmEvent event(date,
				                  mPrefix->text() + aItem->text(AddresseeItem::Name) + mSuffix->text(),
				                  mBgColourChoose->color(), KAlarmEvent::MESSAGE, mFlags);
				event.setAudioFile(mSoundPicker->file());
				QValueList<int> months;
				months.append(date.month());
				event.setRecurAnnualByDate(1, months, -1);
				event.setNextOccurrence(QDateTime(today, QTime(12,0,0)));
				list.append(event);
			}
		}
	}
	return list;
}

void BirthdayDlg::slotOk()
{
	// Save prefix and suffix texts to use as future defaults
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));
	config->writeEntry(QString::fromLatin1("BirthdayPrefix"), mPrefix->text());
	config->writeEntry(QString::fromLatin1("BirthdaySuffix"), mSuffix->text());
	config->sync();

	mFlags = (mSoundPicker->beep()     ? KAlarmEvent::BEEP : 0)
	       | (mLateCancel->isChecked() ? KAlarmEvent::LATE_CANCEL : 0)
	       | (mConfirmAck->isChecked() ? KAlarmEvent::CONFIRM_ACK : 0)
	       |                             KAlarmEvent::ANY_TIME;
	KDialogBase::slotOk();
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the OK button depending on whether anything is selected.
*/
void BirthdayDlg::slotSelectionChanged()
{
	for (QListViewItem* item = mAddresseeList->firstChild();  item;  item = item->nextSibling())
		if (mAddresseeList->isSelected(item))
		{
			enableButtonOK(true);
			return;
		}
	enableButtonOK(false);
	
}


AddresseeItem::AddresseeItem(QListView* parent, const Addressee& addressee)
	: QListViewItem(parent),
	  mAddressee(addressee)
{
	QString name = addressee.nickName();
	if (name.isEmpty())
		name = addressee.realName();
	setText(Name, name);
	QDate date = addressee.birthday().date();
	setText(Birthday, KGlobal::locale()->formatDate(date, true));
	mBirthdayOrder.sprintf("%04d%03d", date.year(), date.dayOfYear());
}

QString AddresseeItem::key(int column, bool) const
{
	if (column == Birthday)
		return mBirthdayOrder;
	return text(column).lower();
}
