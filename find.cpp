/*
 *  find.cpp  -  search facility 
 *  Program:  kalarm
 *  (C) 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qwhatsthis.h>
#include <qgroupbox.h>
#include <qcheckbox.h>

#include <kfinddialog.h>
#include <kfind.h>
#include <kseparator.h>
#include <kwin.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include "alarmlistview.h"
#include "preferences.h"
#include "find.moc"

// KAlarm-specific options for Find dialog
enum {
	FIND_LIVE    = KFindDialog::MinimumUserOption,
	FIND_EXPIRED = KFindDialog::MinimumUserOption << 1,
	FIND_MESSAGE = KFindDialog::MinimumUserOption << 2,
	FIND_FILE    = KFindDialog::MinimumUserOption << 3,
	FIND_COMMAND = KFindDialog::MinimumUserOption << 4,
	FIND_EMAIL   = KFindDialog::MinimumUserOption << 5
};
static long FIND_KALARM_OPTIONS = FIND_LIVE | FIND_EXPIRED | FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL;


Find::Find(EventListViewBase* parent)
	: QObject(parent),
	  mListView(parent),
	  mDialog(0),
	  mFind(0),
	  mOptions(0)
{
}

Find::~Find()
{
	delete mDialog;
	mDialog = 0;
	delete mFind;
	mFind = 0;
}

/******************************************************************************
*  Display the Find dialog.
*/
void Find::display()
{
	if (!mOptions)
		// Set defaults the first time the Find dialog is activated
		mOptions = FIND_LIVE | FIND_EXPIRED | FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL;
	bool noExpired = !Preferences::instance()->expiredKeepDays();
	bool showExpired = mListView->isA("AlarmListView") && ((AlarmListView*)mListView)->showingExpired();
	if (noExpired  ||  !showExpired)      // these settings could change between activations
		mOptions &= ~FIND_EXPIRED;

	if (mDialog)
	{
		KWin::activateWindow(mDialog->winId());
	}
	else
	{
		mDialog = new KFindDialog(false, mListView, "findDlg", mOptions, mHistory, (mListView->selectedCount() > 1));
		mDialog->setHasSelection(false);
		QWidget* kalarmWidgets = mDialog->findExtension();

		// Alarm types
		QBoxLayout* layout = new QVBoxLayout(kalarmWidgets, 0, KDialog::spacingHint());
		QGroupBox* group = new QGroupBox(i18n("Alarm Type"), kalarmWidgets);
		layout->addWidget(group);
		QGridLayout* grid = new QGridLayout(group, 2, 2, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
		grid->addRowSpacing(0, mDialog->fontMetrics().lineSpacing()/2);
		grid->setColStretch(1, 1);

		// Live & expired alarm selection
		mLive = new QCheckBox(i18n("Acti&ve"), group);
		mLive->setFixedSize(mLive->sizeHint());
		QWhatsThis::add(mLive, i18n("Check to include active alarms in the search."));
		grid->addWidget(mLive, 1, 0, Qt::AlignAuto);

		mExpired = new QCheckBox(i18n("Ex&pired"), group);
		mExpired->setFixedSize(mExpired->sizeHint());
		QWhatsThis::add(mExpired,
		      i18n("Check to include expired alarms in the search. "
		           "This option is only available if expired alarms are currently being displayed."));
		grid->addWidget(mExpired, 1, 2, Qt::AlignAuto);

		mActiveExpiredSep = new KSeparator(Qt::Horizontal, kalarmWidgets);
		grid->addMultiCellWidget(mActiveExpiredSep, 2, 2, 0, 2);

		// Alarm actions
		mMessageType = new QCheckBox(i18n("Text"), group, "message");
		mMessageType->setFixedSize(mMessageType->sizeHint());
		QWhatsThis::add(mMessageType, i18n("Check to include text message alarms in the search."));
		grid->addWidget(mMessageType, 3, 0);

		mFileType = new QCheckBox(i18n("Fi&le"), group, "file");
		mFileType->setFixedSize(mFileType->sizeHint());
		QWhatsThis::add(mFileType, i18n("Check to include file alarms in the search."));
		grid->addWidget(mFileType, 3, 2);

		mCommandType = new QCheckBox(i18n("Co&mmand"), group, "command");
		mCommandType->setFixedSize(mCommandType->sizeHint());
		QWhatsThis::add(mCommandType, i18n("Check to include command alarms in the search."));
		grid->addWidget(mCommandType, 4, 0);

		mEmailType = new QCheckBox(i18n("&Email"), group, "email");
		mEmailType->setFixedSize(mEmailType->sizeHint());
		QWhatsThis::add(mEmailType, i18n("Check to include email alarms in the search."));
		grid->addWidget(mEmailType, 4, 2);

		// Set defaults
		mLive->setChecked(mOptions & FIND_LIVE);
		mExpired->setChecked(mOptions & FIND_EXPIRED);
		mMessageType->setChecked(mOptions & FIND_MESSAGE);
		mFileType->setChecked(mOptions & FIND_FILE);
		mCommandType->setChecked(mOptions & FIND_COMMAND);
		mEmailType->setChecked(mOptions & FIND_EMAIL);

		connect(mDialog, SIGNAL(okClicked()), this, SLOT(slotFind()) );
		connect(mDialog, SIGNAL(destroyed()), this, SLOT(slotDlgDestroyed()) );
	}

	// Only display active/expired options if expired alarms are being kept
	if (noExpired)
	{
		mLive->hide();
		mExpired->hide();
		mActiveExpiredSep->hide();
	}
	else
	{
		mLive->show();
		mExpired->show();
		mActiveExpiredSep->show();
	}

	// Disable options where no displayed alarms match them
	bool live    = false;
	bool expired = false;
	bool text    = false;
	bool file    = false;
	bool command = false;
	bool email   = false;
	for (EventListViewItemBase* item = mListView->firstChild();  item;  item = item->nextSibling())
	{
		const KAEvent& event = item->event();
		if (event.expired())
			expired = true;
		else
			live = true;
		switch (event.action())
		{
			case KAEvent::MESSAGE:  text    = true;  break;
			case KAEvent::FILE:     file    = true;  break;
			case KAEvent::COMMAND:  command = true;  break;
			case KAEvent::EMAIL:    email   = true;  break;
		}
	}
	mLive->setEnabled(live);
	mExpired->setEnabled(expired);
	mMessageType->setEnabled(text);
	mFileType->setEnabled(file);
	mCommandType->setEnabled(command);
	mEmailType->setEnabled(email);

	mDialog->setHasCursor(mListView->currentItem());
	mDialog->show();
}

/******************************************************************************
*  Called when the find dialog is being destroyed.
*/
void Find::slotDlgDestroyed()
{
	mDialog = 0;
}

/******************************************************************************
*  Called when the user requests a search by clicking the dialog OK button.
*/
void Find::slotFind()
{
	if (!mDialog)
		return;
	mHistory = mDialog->findHistory();    // save search history so that it can be displayed again
	mOptions = mDialog->options() & ~FIND_KALARM_OPTIONS;
	mOptions |= (mLive->isEnabled()        && mLive->isChecked()        ? FIND_LIVE : 0)
	         |  (mExpired->isEnabled()     && mExpired->isChecked()     ? FIND_EXPIRED : 0)
	         |  (mMessageType->isEnabled() && mMessageType->isChecked() ? FIND_MESSAGE : 0)
	         |  (mFileType->isEnabled()    && mFileType->isChecked()    ? FIND_FILE : 0)
	         |  (mCommandType->isEnabled() && mCommandType->isChecked() ? FIND_COMMAND : 0)
	         |  (mEmailType->isEnabled()   && mEmailType->isChecked()   ? FIND_EMAIL : 0);
	if (!(mOptions & (FIND_LIVE | FIND_EXPIRED))
	||  !(mOptions & (FIND_MESSAGE | FIND_FILE | FIND_COMMAND | FIND_EMAIL)))
	{
		KMessageBox::sorry(mDialog, i18n("No alarm types are selected to search"));
		return;
	}

	// Supply KFind with only those options which relate to the text within alarms
	long options = mOptions & (KFindDialog::WholeWordsOnly | KFindDialog::CaseSensitive | KFindDialog::RegularExpression);
	if (mFind)
	{
		delete mDialog;
		mDialog = 0;
		mFind->setOptions(options);
		findNext(true, true, false);
	}
	else
	{
		mFind = new KFind(mDialog->pattern(), options, mListView, mDialog);
		connect(mFind, SIGNAL(destroyed()), SLOT(slotKFindDestroyed()));
		delete mDialog;                  // close the Find dialogue
		mDialog = 0;
		mFind->closeFindNextDialog();    // prevent 'Find Next' dialog appearing

		// Set the starting point for the search
		mListView->sort();     // ensure the whole list is sorted, not just the visible items
		mStartID       = QString::null;
		mNoCurrentItem = true;
		bool fromCurrent = false;
		EventListViewItemBase* item = 0;
		if (mOptions & KFindDialog::FromCursor)
		{
			item = mListView->currentItem();
			if (item)
			{
				mStartID       = item->event().id();
				mNoCurrentItem = false;
				fromCurrent    = true;
			}
		}

		// Execute the search
		mFound = false;
		findNext(true, false, fromCurrent);
		if (mFind)
			emit active(true);
	}
}

/******************************************************************************
*  Perform the search.
*  If 'fromCurrent' is true, the search starts with the current search item;
*  otherwise, it starts from the next item.
*/
void Find::findNext(bool forward, bool sort, bool fromCurrent)
{
	if (sort)
		mListView->sort();    // ensure the whole list is sorted, not just the visible items

	EventListViewItemBase* item = mNoCurrentItem ? 0 : mListView->currentItem();
	if (!fromCurrent)
		item = nextItem(item, forward);

	// Search successive alarms until a match is found or the end is reached
	bool found = false;
	for ( ;  item;  item = nextItem(item, forward))
	{
		const KAEvent& event = item->event();
		if (!fromCurrent  &&  !mStartID.isNull()  &&  mStartID == event.id())
			break;    // we've wrapped round and reached the starting alarm again
		fromCurrent = false;
		bool live = !event.expired();
		if (live  &&  !(mOptions & FIND_LIVE)
		||  !live  &&  !(mOptions & FIND_EXPIRED))
			continue;     // we're not searching this type of alarm
		switch (event.action())
		{
			case KAEvent::MESSAGE:
				if (!(mOptions & FIND_MESSAGE))
					break;
				mFind->setData(event.cleanText());
				found = (mFind->find() == KFind::Match);
				break;

			case KAEvent::FILE:
				if (!(mOptions & FIND_FILE))
					break;
				mFind->setData(event.cleanText());
				found = (mFind->find() == KFind::Match);
				break;

			case KAEvent::COMMAND:
				if (!(mOptions & FIND_COMMAND))
					break;
				mFind->setData(event.cleanText());
				found = (mFind->find() == KFind::Match);
				break;

			case KAEvent::EMAIL:
				if (!(mOptions & FIND_EMAIL))
					break;
				mFind->setData(event.emailAddresses(", "));
				found = (mFind->find() == KFind::Match);
				if (found)
					break;
				mFind->setData(event.emailSubject());
				found = (mFind->find() == KFind::Match);
				if (found)
					break;
				mFind->setData(event.emailAttachments().join(", "));
				found = (mFind->find() == KFind::Match);
				if (found)
					break;
				mFind->setData(event.cleanText());
				found = (mFind->find() == KFind::Match);
				break;
		}
		if (found)
			break;
	}

	// Process the search result
	mNoCurrentItem = !item;
	if (found)
	{
		// A matching alarm was found - highlight it and make it current
		mFound = true;
		mListView->clearSelection();
		mListView->setSelected(item, true);
		mListView->setCurrentItem(item);
		mListView->ensureItemVisible(item);
	}
	else
	{
		// No match was found
		if (mFound)
		{
			QString msg = forward ? i18n("End of alarm list reached.\nContinue from the beginning?")
			                      : i18n("Beginning of alarm list reached.\nContinue from the end?");
			if (KMessageBox::questionYesNo(mListView, msg) == KMessageBox::Yes)
			{
				findNext(forward, false);
				return;
			}
		}
		else
			mFind->displayFinalDialog();     // display "no match was found"
		mNoCurrentItem = false;    // restart from the currently highlighted alarm if Find Next etc selected
	}
}

/******************************************************************************
*  Get the next alarm item to search.
*/
EventListViewItemBase* Find::nextItem(EventListViewItemBase* item, bool forward) const
{
	QListViewItem* it;
	if (mOptions & KFindDialog::FindBackwards)
		forward = !forward;
	if (forward)
		it = item ? item->itemBelow() : mListView->firstChild();
	else
		it = item ? item->itemAbove() : mListView->lastItem();
	return (EventListViewItemBase*)it;
}
