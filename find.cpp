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
#warning Does the desktop need to be set - presumably it is automatically on same as main window?
//		KWin::setOnDesktop(mDialog->winId(), KWin::currentDesktop());
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
	mExpired->setEnabled(showExpired);

	mDialog->setHasCursor(mListView->currentItem());
	mDialog->show();
}

/******************************************************************************
*  Called when the find dialog is being destroyed.
*/
void Find::slotDlgDestroyed()
{
kdDebug()<<"slotDlgDestroyed()\n";
#warning Should mFind be deleted here, or does it happen automatically?
//	delete mFind;
//	mFind   = 0;
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
	mOptions |= (mLive->isChecked()    ? FIND_LIVE : 0)
	         |  (mExpired->isChecked() ? FIND_EXPIRED : 0)
	         |  (mMessageType->isChecked() ? FIND_MESSAGE : 0)
	         |  (mFileType->isChecked() ? FIND_FILE : 0)
	         |  (mCommandType->isChecked() ? FIND_COMMAND : 0)
	         |  (mEmailType->isChecked() ? FIND_EMAIL : 0);

	// Supply KFind with only those options which relate to the text within alarms
	long options = mOptions & (KFindDialog::WholeWordsOnly | KFindDialog::CaseSensitive | KFindDialog::RegularExpression);
	if (mFind)
	{
		mFind->setOptions(options);
		findNext(true, true, false);
	}
	else
	{
		mFind = new KFind(mDialog->pattern(), options, mListView, mDialog);
		connect(mFind, SIGNAL(destroyed()), SLOT(slotKFindDestroyed()));
		mFind->closeFindNextDialog();    // prevent 'Find Next' dialog appearing in addition to the Find dialog

		// Set the starting point for the search
		mListView->sort();     // ensure the whole list is sorted, not just the visible items
		mStartID       = QString::null;
		mFromMiddle    = false;
		mNoCurrentItem = true;
		bool fromCurrent = false;
		EventListViewItemBase* item = 0;
		if (mOptions & KFindDialog::FromCursor)
		{
			item = mListView->currentItem();
			if (item)
			{
				mStartID       = item->event().id();
				mFromMiddle    = (mOptions & KFindDialog::FindBackwards) ? item->itemBelow() : item->itemAbove();
				mNoCurrentItem = false;
				fromCurrent    = true;
			}
		}
if (item) kdDebug()<<"slotFind(): event="<<item->event().id()<<endl;

		// Execute the search
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
if (item) kdDebug()<<"findNext(): event="<<item->event().id()<<endl; else kdDebug()<<"findNext(): event=null\n";

	// Search successive alarms until a match is found or the end is reached
	bool found = false;
	for ( ;  item;  item = nextItem(item, forward))
	{
if (item) kdDebug()<<"findNext() 2: event="<<item->event().id()<<endl; else kdDebug()<<"findNext() 2: event=null\n";
		const KAEvent& event = item->event();
		if (!mStartID.isNull()  &&  mStartID == event.id())
			break;    // we've wrapped round and reached the starting alarm again
kdDebug()<<"findNext() 3\n";
		bool live = !event.expired();
		if (live  &&  !(mOptions & FIND_LIVE)
		||  !live  &&  !(mOptions & FIND_EXPIRED))
			continue;     // we're not searching this type of alarm
		switch (event.action())
		{
			case KAEvent::MESSAGE:
				if (!(mOptions & FIND_MESSAGE))
					break;
kdDebug()<<"findNext() 4: message="<<event.cleanText()<<endl;
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
if (item) kdDebug()<<"findNext() end: item="<<item->event().id()<<endl; else kdDebug()<<"findNext(): end: event=null\n";
	mNoCurrentItem = !item;
	if (found)
	{
		// A matching alarm was found - highlight it and make it current
		mListView->clearSelection();
		mListView->setSelected(item, true);
		mListView->setCurrentItem(item);
		mListView->ensureItemVisible(item);
	}
	else
	{
		// No match was found
		if (mFromMiddle)
		{
			// We haven't yet searched all alarms, so ask user whether to wrap round
			if (mFind->shouldRestart(false, false))
			{
				mFromMiddle = false;
				findNext(forward, false);
				return;
			}
		}
		else
			mFind->displayFinalDialog();     // display "no match was found"
		delete mFind;
		mFind = 0;
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
