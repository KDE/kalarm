/*
 *  birthdaydlg.cpp  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
#if KDE_VERSION >= 290
#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#else
#include <kabapi.h>
#include <addressbook.h>
#include <kglobal.h>
#include <kmessagebox.h>
#endif
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


class AddresseeItem : public QListViewItem
{
	public:
		enum columns { NAME = 0, BIRTHDAY = 1 };
		AddresseeItem(QListView* parent, const QString& name, const QDate& birthday);
		QDate birthday() const   { return mBirthday; }
		virtual QString key(int column, bool ascending) const;
	private:
		QDate     mBirthday;
		QString   mBirthdayOrder;
};



BirthdayDlg::BirthdayDlg(QWidget* parent)
	: KDialogBase(KDialogBase::Plain, i18n("Import Birthdays from KAddressBook"), Ok|Cancel, Ok, parent)
{
	QWidget* topWidget = plainPage();
	QBoxLayout* topLayout = new QVBoxLayout(topWidget);
	topLayout->setSpacing(spacingHint());

	// Prefix and suffix to the name in the alarm text
	// Get default prefix and suffix texts from config file
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));
	mPrefixText = config->readEntry(QString::fromLatin1("BirthdayPrefix"), i18n("Birthday: "));
	mSuffixText = config->readEntry(QString::fromLatin1("BirthdaySuffix"));

	QGroupBox* textGroup = new QGroupBox(2, Qt::Horizontal, i18n("Alarm Text"), topWidget);
	topLayout->addWidget(textGroup);
	QLabel* label = new QLabel(i18n("&Prefix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	mPrefix = new BLineEdit(mPrefixText, textGroup);
	mPrefix->setMinimumSize(mPrefix->sizeHint());
	label->setBuddy(mPrefix);
	connect(mPrefix, SIGNAL(lostFocus()), SLOT(slotTextLostFocus()));
	QWhatsThis::add(mPrefix,
	      i18n("Enter text to appear before the person's name in the alarm message, "
	           "including any necessary trailing spaces."));

	label = new QLabel(i18n("S&uffix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	mSuffix = new BLineEdit(mSuffixText, textGroup);
	mSuffix->setMinimumSize(mSuffix->sizeHint());
	label->setBuddy(mSuffix);
	connect(mSuffix, SIGNAL(textChanged()), SLOT(slotTextChanged()));
	QWhatsThis::add(mSuffix,
	      i18n("Enter text to appear after the person's name in the alarm message, "
	           "including any necessary leading spaces."));

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

	// How much to advance warning to give
	QBoxLayout* layout = new QHBoxLayout(groupLayout);
	mReminder = new CheckBox(i18n("&Reminder"), group);
	mReminder->setFixedSize(mReminder->sizeHint());
	QWhatsThis::add(mReminder,
	      i18n("Check to display a reminder in advance of the birthday."));
	layout->addWidget(mReminder);
	mReminderTime = new SpinBox(1, 364, 1, group);
	mReminderTime->setValue(364);
	mReminderTime->setFixedSize(mReminderTime->sizeHint());
	mReminderTime->setValue(1);
	QWhatsThis::add(mReminderTime,
	      i18n("Enter the number of days before each birthday to display a reminder."
	           "This is in addition to the alarm which is displayed on the birthday."));
	mReminder->setFocusWidget(mReminderTime);
	layout->addWidget(mReminderTime);
	label = new QLabel(i18n("days before"), group);
	label->setFixedSize(label->sizeHint());
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

	// Initialise the birthday selection list and disable the OK button
	updateSelectionList();
}

BirthdayDlg::~BirthdayDlg()
{
}

/******************************************************************************
* Initialise or update the birthday selection list by fetching all birthdays
* from the address book and displaying those which do not already have alarms.
*/
void BirthdayDlg::updateSelectionList()
{
	// Compile a list of all pending alarm messages which look like birthdays
	QStringList messageList;
	KAlarmEvent event;
	QPtrList<Event> events = theApp()->getCalendar().events();
	for (Event* kcalEvent = events.first();  kcalEvent;  kcalEvent = events.next())
	{
		event.set(*kcalEvent);
		if (event.action() == KAlarmEvent::MESSAGE
		&&  event.recurType() == KAlarmEvent::ANNUAL_DATE
		&&  (mPrefixText.isEmpty()  ||  event.message().startsWith(mPrefixText)))
			messageList.append(event.message());
	}

	mAddresseeList->setUpdatesEnabled(false);

	// Fetch all birthdays from the address book
#if KDE_VERSION >= 290
	mAddressBook = KABC::StdAddressBook::self();
	for (KABC::AddressBook::Iterator it = mAddressBook->begin();  it != mAddressBook->end();  ++it)
	{
		const KABC::Addressee& addressee = *it;
		if (addressee.birthday().isValid())
		{
			// Create a list entry for this birthday
			QDate birthday = addressee.birthday().date();
			QString name = addressee.nickName();
			if (name.isEmpty())
				name = addressee.realName();
#else
	bool err = false;
	KabAPI addrDialog;
	if (addrDialog.init() != AddressBook::NoError)
		err = true;
	else
	{
		AddressBook* addrBook = addrDialog.addressbook();
		std::list<AddressBook::Entry> entries;
		AddressBook::ErrorCode errcode = addrBook->getEntries(entries);
		if (errcode != AddressBook::NoError  &&  errcode != AddressBook::NoEntry)
			err = true;
		else if (errcode != AddressBook::NoEntry)
		{
kdDebug()<<"BirthdayDlg: iterating\n";
			for (std::list<AddressBook::Entry>::iterator it = entries.begin();  it != entries.end();  ++it)
			{
kdDebug()<<"BirthdayDlg: name="<<it->firstname<<" "<<it->lastname<<endl;
kdDebug()<<"BirthdayDlg: birthday="<<it->birthday.toString()<<endl;
				if (it->birthday.isValid())
				{
					// Create a list entry for this birthday
					QDate birthday = it->birthday;
					QString name;
					addrBook->literalName(*it, name);
#endif
			// Check if the birthday already has an alarm
			QString text = mPrefixText + name + mSuffixText;
			bool alarmExists = (messageList.find(text) != messageList.end());
			// Check if the birthday is already in the selection list
			bool inSelectionList = false;
			AddresseeItem* item;
			for (QListViewItem* qitem = mAddresseeList->firstChild();  qitem;  qitem = qitem->nextSibling())
			{
				item = dynamic_cast<AddresseeItem*>(qitem);
				inSelectionList = (item  &&  item->text(AddresseeItem::NAME) == name  &&  item->birthday() == birthday);
			}

			if (alarmExists  &&  inSelectionList)
				delete item;     // alarm exists, so remove from selection list
			else if (!alarmExists  &&  !inSelectionList)
				new AddresseeItem(mAddresseeList, name, birthday);   // add to list
#if KDE_VERSION >= 290
		}
	}
#else
				}
			}
		}
	}
	if (err)
		KMessageBox::error(this, i18n("Unable to open address book"));
#endif
	mAddresseeList->setUpdatesEnabled(true);

	// Enable/disable OK button according to whether anything is currently selected
	bool selection = false;
	for (QListViewItem* item = mAddresseeList->firstChild();  item;  item = item->nextSibling())
		if (mAddresseeList->isSelected(item))
		{
			selection = true;
			break;
		}
	enableButtonOK(selection);
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
				QDate date = aItem->birthday();
				date.setYMD(thisYear, date.month(), date.day());
				if (date <= today)
					date.setYMD(thisYear + 1, date.month(), date.day());
				KAlarmEvent event(date,
				                  mPrefix->text() + aItem->text(AddresseeItem::NAME) + mSuffix->text(),
				                  mBgColourChoose->color(), KAlarmEvent::MESSAGE, mFlags);
				event.setAudioFile(mSoundPicker->file());
				QValueList<int> months;
				months.append(date.month());
				event.setRecurAnnualByDate(1, months, -1);
				event.setNextOccurrence(QDateTime(today, QTime(12,0,0)));
				if (mReminder->isChecked())
					event.setReminder(mReminderTime->value() * 1440);
				list.append(event);
			}
		}
	}
	return list;
}

/******************************************************************************
* Called when the OK button is selected to import the selected birthdays.
*/
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

/******************************************************************************
* Called when the prefix or suffix text has lost keyboard focus.
* If the text has changed, re-evaluates the selection list according to the new
* birthday alarm text format.
*/
void BirthdayDlg::slotTextLostFocus()
{
	QString prefix = mPrefix->text();
	QString suffix = mSuffix->text();
	if (prefix != mPrefixText  ||  suffix != mSuffixText)
	{
		// Text has changed - re-evaluate the selection list
		mPrefixText = prefix;
		mSuffixText = suffix;
		updateSelectionList();
	}
}


/*=============================================================================
= Class: AddresseeItem
=============================================================================*/

AddresseeItem::AddresseeItem(QListView* parent, const QString& name, const QDate& birthday)
	: QListViewItem(parent),
	  mBirthday(birthday)
{
	setText(NAME, name);
	setText(BIRTHDAY, KGlobal::locale()->formatDate(mBirthday, true));
	mBirthdayOrder.sprintf("%04d%03d", mBirthday.year(), mBirthday.dayOfYear());
}

QString AddresseeItem::key(int column, bool) const
{
	if (column == BIRTHDAY)
		return mBirthdayOrder;
	return text(column).lower();
}
