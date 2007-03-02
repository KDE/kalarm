/*
 *  birthdaydlg.cpp  -  dialog to pick birthdays from address book
 *  Program:  kalarm
 *  Copyright © 2002-2007 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include <QGroupBox>
#include <QLabel>
#include <QTreeView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <klocale.h>
#include <kglobal.h>
#include <kconfiggroup.h>
#include <kmessagebox.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <khbox.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "birthdaymodel.h"
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


BirthdayDlg::BirthdayDlg(QWidget* parent)
	: KDialog(parent),
	  mSpecialActionsButton(0)
{
	setObjectName("BirthdayDlg");    // used by LikeBack
	setCaption(i18n("Import Birthdays From KAddressBook"));
	setButtons(Ok | Cancel);
	setDefaultButton(Ok);
	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));

	QWidget* topWidget = new QWidget(this);
	setMainWidget(topWidget);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacingHint());

	// Prefix and suffix to the name in the alarm text
	// Get default prefix and suffix texts from config file
	KConfigGroup config(KGlobal::config(), "General");
	mPrefixText = config.readEntry("BirthdayPrefix", i18n("Birthday: "));
	mSuffixText = config.readEntry("BirthdaySuffix");

	QGroupBox* textGroup = new QGroupBox(i18n("Alarm Text"), topWidget);
	topLayout->addWidget(textGroup);
	QGridLayout* grid = new QGridLayout(textGroup);
	grid->setMargin(marginHint());
	grid->setSpacing(spacingHint());
	QLabel* label = new QLabel(i18n("Pre&fix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0);
	mPrefix = new BLineEdit(mPrefixText, textGroup);
	mPrefix->setMinimumSize(mPrefix->sizeHint());
	label->setBuddy(mPrefix);
	connect(mPrefix, SIGNAL(focusLost()), SLOT(slotTextLostFocus()));
	mPrefix->setWhatsThis(i18n("Enter text to appear before the person's name in the alarm message, "
	                           "including any necessary trailing spaces."));
	grid->addWidget(mPrefix, 0, 1);

	label = new QLabel(i18n("S&uffix:"), textGroup);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0);
	mSuffix = new BLineEdit(mSuffixText, textGroup);
	mSuffix->setMinimumSize(mSuffix->sizeHint());
	label->setBuddy(mSuffix);
	connect(mSuffix, SIGNAL(focusLost()), SLOT(slotTextLostFocus()));
	mSuffix->setWhatsThis(i18n("Enter text to appear after the person's name in the alarm message, "
	                           "including any necessary leading spaces."));
	grid->addWidget(mSuffix, 1, 1);

	QGroupBox* group = new QGroupBox(i18n("Select Birthdays"), topWidget);
	topLayout->addWidget(group);
	QVBoxLayout* layout = new QVBoxLayout(group);
	layout->setMargin(0);
	BirthdayModel* model = BirthdayModel::instance(this);
	connect(model, SIGNAL(addrBookError()), SLOT(addrBookError()));
	connect(model, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)), SLOT(resizeViewColumns()));
	model->setPrefixSuffix(mPrefixText, mSuffixText);
	BirthdaySortModel* sortModel = new BirthdaySortModel(model, this);
	sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	mListView = new QTreeView(group);
	mListView->setModel(sortModel);
	mListView->setRootIsDecorated(false);    // don't show expander icons
	mListView->setSortingEnabled(true);
	mListView->sortByColumn(BirthdayModel::NameColumn);
	mListView->setAllColumnsShowFocus(true);
	mListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	mListView->setSelectionBehavior(QAbstractItemView::SelectRows);
	mListView->setTextElideMode(Qt::ElideRight);
	mListView->header()->setResizeMode(BirthdayModel::NameColumn, QHeaderView::Stretch);
	mListView->header()->setResizeMode(BirthdayModel::DateColumn, QHeaderView::ResizeToContents);
	connect(mListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), SLOT(slotSelectionChanged()));
	mListView->setWhatsThis(i18n("Select birthdays to set alarms for.\n"
	                             "This list shows all birthdays in KAddressBook except those for which alarms already exist.\n\n"
	                             "You can select multiple birthdays at one time by dragging the mouse over the list, "
	                             "or by clicking the mouse while pressing Ctrl or Shift."));
	layout->addWidget(mListView);

	group = new QGroupBox(i18n("Alarm Configuration"), topWidget);
	topLayout->addWidget(group);
	QVBoxLayout* groupLayout = new QVBoxLayout(group);
	groupLayout->setMargin(marginHint());
	groupLayout->setSpacing(spacingHint());

	// Colour choice drop-down list
	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(2*spacingHint());
	groupLayout->addLayout(hlayout);
	KHBox* box;
	mBgColourChoose = EditAlarmDlg::createBgColourChooser(&box, group);
	connect(mBgColourChoose, SIGNAL(highlighted(const QColor&)), SLOT(slotBgColourSelected(const QColor&)));
	hlayout->addWidget(box);
	hlayout->addStretch();

	// Font and colour choice drop-down list
	mFontColourButton = new FontColourButton(group);
	mFontColourButton->setFixedSize(mFontColourButton->sizeHint());
	connect(mFontColourButton, SIGNAL(selected()), SLOT(slotFontColourSelected()));
	hlayout->addWidget(mFontColourButton);

	// Sound checkbox and file selector
	mSoundPicker = new SoundPicker(group);
	mSoundPicker->setFixedSize(mSoundPicker->sizeHint());
	groupLayout->addWidget(mSoundPicker, 0, Qt::AlignLeft);

	// How much advance warning to give
	mReminder = new Reminder(i18n("&Reminder"),
	                         i18n("Check to display a reminder in advance of the birthday."),
	                         i18n("Enter the number of days before each birthday to display a reminder. "
	                              "This is in addition to the alarm which is displayed on the birthday."),
	                         false, false, group);
	mReminder->setFixedSize(mReminder->sizeHint());
	mReminder->setMaximum(0, 364);
	mReminder->setMinutes(0, true);
	groupLayout->addWidget(mReminder, 0, Qt::AlignLeft);

	// Acknowledgement confirmation required - default = no confirmation
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(2*spacingHint());
	groupLayout->addLayout(hlayout);
	mConfirmAck = EditAlarmDlg::createConfirmAckCheckbox(group);
	mConfirmAck->setFixedSize(mConfirmAck->sizeHint());
	hlayout->addWidget(mConfirmAck);
	hlayout->addSpacing(2*spacingHint());
	hlayout->addStretch();

	if (ShellProcess::authorised())    // don't display if shell commands not allowed (e.g. kiosk mode)
	{
		// Special actions button
		mSpecialActionsButton = new SpecialActionsButton(i18n("Special Actions..."), group);
		mSpecialActionsButton->setFixedSize(mSpecialActionsButton->sizeHint());
		hlayout->addWidget(mSpecialActionsButton);
	}

	// Late display checkbox - default = allow late display
	hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(2*spacingHint());
	groupLayout->addLayout(hlayout);
	mLateCancel = new LateCancelSelector(false, group);
	mLateCancel->setFixedSize(mLateCancel->sizeHint());
	hlayout->addWidget(mLateCancel);
	hlayout->addStretch();

	// Simple repetition button
	mSimpleRepetition = new RepetitionButton(i18n("Simple Repetition"), false, group);
	mSimpleRepetition->setFixedSize(mSimpleRepetition->sizeHint());
	mSimpleRepetition->set(0, 0, true, 364*24*60);
	mSimpleRepetition->setWhatsThis(i18n("Set up an additional alarm repetition"));
	hlayout->addWidget(mSimpleRepetition);

	// Set the values to their defaults
	mFontColourButton->setDefaultFont();
	mFontColourButton->setBgColour(Preferences::defaultBgColour());
	mBgColourChoose->setColour(Preferences::defaultBgColour());     // set colour before setting alarm type buttons
	mLateCancel->setMinutes(Preferences::defaultLateCancel(), true, TimePeriod::Days);
	mConfirmAck->setChecked(Preferences::defaultConfirmAck());
	mSoundPicker->set(Preferences::defaultSoundType(), Preferences::defaultSoundFile(),
	                  Preferences::defaultSoundVolume(), -1, 0, Preferences::defaultSoundRepeat());
	if (mSpecialActionsButton)
		mSpecialActionsButton->setActions(Preferences::defaultPreAction(), Preferences::defaultPostAction());

	KActionCollection* actions = new KActionCollection(this);
	actions->setAssociatedWidget(mListView);
	KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
	KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);

	enableButtonOk(false);     // only enable OK button when something is selected
}

/******************************************************************************
* Display an error message about failure to load address book.
*/
void BirthdayDlg::addrBookError()
{
	KMessageBox::error(this, i18n("Error reading address book"));
}

/******************************************************************************
* Return a list of events for birthdays chosen.
*/
QList<KAEvent> BirthdayDlg::events() const
{
	QList<KAEvent> list;
	QModelIndexList indexes = mListView->selectionModel()->selectedRows();
	int count = indexes.count();
	if (!count)
		return list;
	QDate today = QDate::currentDate();
	KDateTime todayStart(today, KDateTime::ClockTime);
	int thisYear = today.year();
	int reminder = mReminder->minutes();
	BirthdaySortModel* model = static_cast<const BirthdaySortModel*>(indexes[0].model());
	for (int i = 0;  i < count;  ++i)
	{
		BirthdayModel::Data* data = model->rowData(indexes[i]);
		if (!data)
			continue;
		QDate date = data->birthday;
		date.setYMD(thisYear, date.month(), date.day());
		if (date <= today)
			date.setYMD(thisYear + 1, date.month(), date.day());
		KAEvent event(KDateTime(date, KDateTime::ClockTime),
			      mPrefix->text() + data->name + mSuffix->text(),
			      mBgColourChoose->color(), mFontColourButton->fgColour(),
			      mFontColourButton->font(), KAEvent::MESSAGE, mLateCancel->minutes(),
			      mFlags);
		float fadeVolume;
		int   fadeSecs;
		float volume = mSoundPicker->volume(fadeVolume, fadeSecs);
		event.setAudioFile(mSoundPicker->file().prettyUrl(), volume, fadeVolume, fadeSecs);
		QList<int> months;
		months.append(date.month());
		event.setRecurAnnualByDate(1, months, 0, Preferences::defaultFeb29Type(), -1, QDate());
		event.setRepetition(mSimpleRepetition->interval(), mSimpleRepetition->count());
		event.setNextOccurrence(todayStart);
		if (reminder)
			event.setReminder(reminder, false);
		if (mSpecialActionsButton)
			event.setActions(mSpecialActionsButton->preAction(),
					 mSpecialActionsButton->postAction());
		list.append(event);
	}
	return list;
}

/******************************************************************************
* Called when the OK button is selected to import the selected birthdays.
*/
void BirthdayDlg::slotOk()
{
	// Save prefix and suffix texts to use as future defaults
	KConfigGroup config(KGlobal::config(), "General");
	config.writeEntry("BirthdayPrefix", mPrefix->text());
	config.writeEntry("BirthdaySuffix", mSuffix->text());
	config.sync();

	mFlags = (mSoundPicker->sound() == Preferences::Sound_Beep ? KAEvent::BEEP : 0)
	       | (mSoundPicker->repeat()                     ? KAEvent::REPEAT_SOUND : 0)
	       | (mConfirmAck->isChecked()                   ? KAEvent::CONFIRM_ACK : 0)
	       | (mFontColourButton->defaultFont()           ? KAEvent::DEFAULT_FONT : 0)
	       |                                               KAEvent::ANY_TIME;
	KDialog::accept();
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the OK button depending on whether anything is selected.
*/
void BirthdayDlg::slotSelectionChanged()
{
	enableButtonOk(mListView->selectionModel()->hasSelection());
}

/******************************************************************************
* Called when the data has changed in the birthday list.
* Resize the date column.
*/
void BirthdayDlg::resizeViewColumns()
{
	mListView->resizeColumnToContents(BirthdayModel::DateColumn);
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
		BirthdayModel::instance()->setPrefixSuffix(mPrefixText, mSuffixText);
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
