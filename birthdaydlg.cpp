/*
 *  birthdaydlg.cpp  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include <kglobal.h>
#include <kconfig.h>
#include <kmessagebox.h>
#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "checkbox.h"
#include "colourcombo.h"
#include "editdlg.h"
#include "fontcolourbutton.h"
#include "kalarmapp.h"
#include "latecancel.h"
#include "preferences.h"
#include "reminder.h"
#include "repetition.h"
#include "shellprocess.h"
#include "soundpicker.h"
#include "specialactions.h"
#include "birthdaydlg.moc"

using namespace KCal;


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
	: KDialogBase(KDialogBase::Plain, i18n("Import Birthdays From KAddressBook"), Ok|Cancel, Ok, parent),
	  mSpecialActionsButton(0)
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
	QLabel* label = new QLabel(i18n("Pre&fix:"), textGroup);
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
	connect(mSuffix, SIGNAL(lostFocus()), SLOT(slotTextLostFocus()));
	QWhatsThis::add(mSuffix,
	      i18n("Enter text to appear after the person's name in the alarm message, "
	           "including any necessary leading spaces."));

	QGroupBox* group = new QGroupBox(1, Qt::Horizontal, i18n("Select Birthdays"), topWidget);
	topLayout->addWidget(group);
	mAddresseeList = new KListView(group);
	mAddresseeList->setMultiSelection(true);
	mAddresseeList->setSelectionMode(QListView::Extended);
	mAddresseeList->setAllColumnsShowFocus(true);
#if KDE_VERSION >= 290
	mAddresseeList->setFullWidth(true);
#endif
	mAddresseeList->addColumn(i18n("Name"));
	mAddresseeList->addColumn(i18n("Birthday"));
	connect(mAddresseeList, SIGNAL(selectionChanged()), SLOT(slotSelectionChanged()));
	QWhatsThis::add(mAddresseeList,
	      i18n("Select birthdays to set alarms for.\n"
	           "This list shows all birthdays in KAddressBook except those for which alarms already exist.\n\n"
	           "You can select multiple birthdays at one time by dragging the mouse over the list, "
	           "or by clicking the mouse while pressing Ctrl or Shift."));

	group = new QGroupBox(i18n("Alarm Configuration"), topWidget);
	topLayout->addWidget(group);
	QBoxLayout* groupLayout = new QVBoxLayout(group, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	groupLayout->addSpacing(fontMetrics().lineSpacing()/2);

	// Colour choice drop-down list
	QBoxLayout* layout = new QHBoxLayout(groupLayout, 2*spacingHint());
	QHBox* box;
	mBgColourChoose = EditAlarmDlg::createBgColourChooser(&box, group);
	connect(mBgColourChoose, SIGNAL(highlighted(const QColor&)), SLOT(slotBgColourSelected(const QColor&)));
	layout->addWidget(box);
	layout->addStretch();

	// Font and colour choice drop-down list
	mFontColourButton = new FontColourButton(group);
	mFontColourButton->setFixedSize(mFontColourButton->sizeHint());
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	layout->addWidget(mFontColourButton);

	// Sound checkbox and file selector
	mSoundPicker = new SoundPicker(group);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	groupLayout->addWidget(mSoundPicker, 0, Qt::AlignAuto);

	// How much to advance warning to give
	mReminder = new Reminder(i18n("&Reminder"),
	                         i18n("Check to display a reminder in advance of the birthday."),
	                         i18n("Enter the number of days before each birthday to display a reminder. "
	                              "This is in addition to the alarm which is displayed on the birthday."),
	                         false, false, group);
	mReminder->setFixedSize(mReminder->sizeHint());
	mReminder->setMaximum(0, 364);
	mReminder->setMinutes(0, true);
	groupLayout->addWidget(mReminder, 0, Qt::AlignAuto);

	// Acknowledgement confirmation required - default = no confirmation
	layout = new QHBoxLayout(groupLayout, 2*spacingHint());
	mConfirmAck = EditAlarmDlg::createConfirmAckCheckbox(group);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	layout->addWidget(mConfirmAck);
	layout->addSpacing(2*KDialog::spacingHint());
	layout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(i18n("Special Actions..."), group);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		layout->addWidget(mSpecialActionsButton);
	}

	// Late display checkbox - default = allow late display
	layout = new QHBoxLayout(groupLayout, 2*spacingHint());
	mLateCancel = new LateCancelSelector(false, group);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	layout->addWidget(mLateCancel);
	layout->addStretch();

	// Simple repetition button
	mSimpleRepetition = new RepetitionButton(i18n("Simple Repetition"), false, group);
	mSimpleRepetition->setFixedSize(mSimpleRepetition->sizeHint());
	mSimpleRepetition->set(0, 0, true, 364*24*60);
	QWhatsThis::add(mSimpleRepetition, i18n("Set up an additional alarm repetition"));
	layout->addWidget(mSimpleRepetition);

	// Set the values to their defaults
	Preferences* preferences = Preferences::instance();
	mFontColourButton->setDefaultFont();
	mFontColourButton->setBgColour(preferences->defaultBgColour());
	mBgColourChoose->setColour(preferences->defaultBgColour());     // set colour before setting alarm type buttons
	mLateCancel->setMinutes(preferences->defaultLateCancel(), true, TimePeriod::DAYS);
	mConfirmAck->setChecked(preferences->defaultConfirmAck());
	mSoundPicker->set(preferences->defaultBeep(), preferences->defaultSoundFile(),
	                  preferences->defaultSoundVolume(), -1, 0, preferences->defaultSoundRepeat());
	if (mSpecialActionsButton)
		mSpecialActionsButton->setActions(preferences->defaultPreAction(), preferences->defaultPostAction());

	// Initialise the birthday selection list and disable the OK button
	updateSelectionList();
}

/******************************************************************************
* Initialise or update the birthday selection list by fetching all birthdays
* from the address book and displaying those which do not already have alarms.
*/
void BirthdayDlg::updateSelectionList()
{
	// Compile a list of all pending alarm messages which look like birthdays
	QStringList messageList;
	KAEvent event;
	Event::List events = AlarmCalendar::activeCalendar()->events();
	for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
	{
		Event* kcalEvent = *it;
		event.set(*kcalEvent);
		if (event.action() == KAEvent::MESSAGE
		&&  event.recurType() == KAEvent::ANNUAL_DATE
		&&  (mPrefixText.isEmpty()  ||  event.message().startsWith(mPrefixText)))
			messageList.append(event.message());
	}

	// Fetch all birthdays from the address book
	mAddressBook = KABC::StdAddressBook::self( true );
	if (!mAddressBook)
		KMessageBox::error(this, i18n("Error reading address book"));
	else
	{
		for (KABC::AddressBook::ConstIterator abit = mAddressBook->begin();  abit != mAddressBook->end();  ++abit)
		{
			const KABC::Addressee& addressee = *abit;
			if (addressee.birthday().isValid())
			{
				// Create a list entry for this birthday
				QDate birthday = addressee.birthday().date();
				QString name = addressee.nickName();
				if (name.isEmpty())
					name = addressee.realName();
				// Check if the birthday already has an alarm
				QString text = mPrefixText + name + mSuffixText;
				bool alarmExists = (messageList.find(text) != messageList.end());
				// Check if the birthday is already in the selection list
				bool inSelectionList = false;
				AddresseeItem* item = 0;
				for (QListViewItem* qitem = mAddresseeList->firstChild();  qitem;  qitem = qitem->nextSibling())
				{
					item = dynamic_cast<AddresseeItem*>(qitem);
					if (item  &&  item->text(AddresseeItem::NAME) == name  &&  item->birthday() == birthday)
					{
						inSelectionList = true;
						break;
					}
				}

				if (alarmExists  &&  inSelectionList)
					delete item;     // alarm exists, so remove from selection list
				else if (!alarmExists  &&  !inSelectionList)
					new AddresseeItem(mAddresseeList, name, birthday);   // add to list
			}
		}
	}
//	mAddresseeList->setUpdatesEnabled(true);

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
QValueList<KAEvent> BirthdayDlg::events() const
{
	QValueList<KAEvent> list;
	QDate today = QDate::currentDate();
	QDateTime todayNoon(today, QTime(12, 0, 0));
	int thisYear = today.year();
	int reminder = mReminder->minutes();

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
				KAEvent event(date,
				              mPrefix->text() + aItem->text(AddresseeItem::NAME) + mSuffix->text(),
				              mBgColourChoose->color(), mFontColourButton->fgColour(),
				              mFontColourButton->font(), KAEvent::MESSAGE, mLateCancel->minutes(),
				              mFlags);
				float fadeVolume;
				int   fadeSecs;
				float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
				event.setAudioFile(mSoundPicker->file(), volume, fadeVolume, fadeSecs);
				QValueList<int> months;
				months.append(date.month());
				event.setRecurAnnualByDate(1, months, 0, -1);
				event.setNextOccurrence(todayNoon, true);
				event.setRepetition(mSimpleRepetition->interval(), mSimpleRepetition->count());
				if (reminder)
					event.setReminder(reminder, false);
				if (mSpecialActionsButton)
					event.setActions(mSpecialActionsButton->preAction(),
					                 mSpecialActionsButton->postAction());
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

	mFlags = (mSoundPicker->beep()             ? KAEvent::BEEP : 0)
	       | (mSoundPicker->repeat()           ? KAEvent::REPEAT_SOUND : 0)
	       | (mConfirmAck->isChecked()         ? KAEvent::CONFIRM_ACK : 0)
	       | (mFontColourButton->defaultFont() ? KAEvent::DEFAULT_FONT : 0)
	       |                                     KAEvent::ANY_TIME;
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

/******************************************************************************
*  Called when the a new background colour has been selected using the colour
*  combo box.
*/
void BirthdayDlg::slotBgColourSelected(const QColor& colour)
{
	mFontColourButton->setBgColour(colour);
}

/******************************************************************************
*  Called when the a new font and colour have been selected using the font &
*  colour pushbutton.
*/
void BirthdayDlg::slotFontColourSelected()
{
	mBgColourChoose->setColour(mFontColourButton->bgColour());
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
