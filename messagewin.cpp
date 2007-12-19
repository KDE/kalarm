/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <stdlib.h>
#include <string.h>

#include <qfile.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qdragobject.h>
#include <qtextedit.h>
#include <qtimer.h>

#include <kstandarddirs.h>
#include <kaction.h>
#include <kstdguiitem.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <ktextbrowser.h>
#include <kglobalsettings.h>
#include <kmimetype.h>
#include <kmessagebox.h>
#include <kwin.h>
#include <kwinmodule.h>
#include <kprocess.h>
#include <kio/netaccess.h>
#include <knotifyclient.h>
#include <kpushbutton.h>
#ifdef WITHOUT_ARTS
#include <kaudioplayer.h>
#else
#include <arts/kartsdispatcher.h>
#include <arts/kartsserver.h>
#include <arts/kplayobjectfactory.h>
#include <arts/kplayobject.h>
#endif
#include <dcopclient.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "deferdlg.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "synchtimer.h"
#include "messagewin.moc"

using namespace KCal;

#ifndef WITHOUT_ARTS
static const char* KMIX_APP_NAME    = "kmix";
static const char* KMIX_DCOP_OBJECT = "Mixer0";
static const char* KMIX_DCOP_WINDOW = "kmix-mainwindow#1";
#endif
static const char* KMAIL_DCOP_OBJECT = "KMailIface";

// The delay for enabling message window buttons if a zero delay is
// configured, i.e. the windows are placed far from the cursor.
static const int proximityButtonDelay = 1000;    // (milliseconds)
static const int proximityMultiple = 10;         // multiple of button height distance from cursor for proximity

// A text label widget which can be scrolled and copied with the mouse
class MessageText : public QTextEdit
{
	public:
		MessageText(const QString& text, const QString& context = QString::null, QWidget* parent = 0, const char* name = 0)
		: QTextEdit(text, context, parent, name)
		{
			setReadOnly(true);
			setWordWrap(QTextEdit::NoWrap);
		}
		int scrollBarHeight() const     { return horizontalScrollBar()->height(); }
		int scrollBarWidth() const      { return verticalScrollBar()->width(); }
		virtual QSize sizeHint() const  { return QSize(contentsWidth() + scrollBarWidth(), contentsHeight() + scrollBarHeight()); }
};


class MWMimeSourceFactory : public QMimeSourceFactory
{
	public:
		MWMimeSourceFactory(const QString& absPath, KTextBrowser*);
		virtual ~MWMimeSourceFactory();
		virtual const QMimeSource* data(const QString& abs_name) const;
	private:
		// Prohibit the following methods
		virtual void setData(const QString&, QMimeSource*) {}
		virtual void setExtensionType(const QString&, const char*) {}

		QString   mTextFile;
		QCString  mMimeType;
		mutable const QMimeSource* mLast;
};


// Basic flags for the window
static const Qt::WFlags WFLAGS = Qt::WStyle_StaysOnTop | Qt::WDestructiveClose;

// Error message bit masks
enum {
	ErrMsg_Speak     = 0x01,
	ErrMsg_AudioFile = 0x02,
	ErrMsg_Volume    = 0x04
};


QValueList<MessageWin*> MessageWin::mWindowList;
QMap<QString, unsigned> MessageWin::mErrorMessages;


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAEvent& event, const KAAlarm& alarm, bool reschedule_event, bool allowDefer)
	: MainWindowBase(0, "MessageWin", WFLAGS | Qt::WGroupLeader | Qt::WStyle_ContextHelp
	                                         | (Preferences::modalMessages() ? 0 : Qt::WX11BypassWM)),
	  mMessage(event.cleanText()),
	  mFont(event.font()),
	  mBgColour(event.bgColour()),
	  mFgColour(event.fgColour()),
	  mDateTime((alarm.type() & KAAlarm::REMINDER_ALARM) ? event.mainDateTime(true) : alarm.dateTime(true)),
	  mEventID(event.id()),
	  mAudioFile(event.audioFile()),
	  mVolume(event.soundVolume()),
	  mFadeVolume(event.fadeVolume()),
	  mFadeSeconds(QMIN(event.fadeSeconds(), 86400)),
	  mDefaultDeferMinutes(event.deferDefaultMinutes()),
	  mAlarmType(alarm.type()),
	  mAction(event.action()),
	  mKMailSerialNumber(event.kmailSerialNumber()),
	  mRestoreHeight(0),
	  mAudioRepeat(event.repeatSound()),
	  mConfirmAck(event.confirmAck()),
	  mShowEdit(!mEventID.isEmpty()),
	  mNoDefer(!allowDefer || alarm.repeatAtLogin()),
	  mInvalid(false),
	  mArtsDispatcher(0),
	  mPlayObject(0),
	  mOldVolume(-1),
	  mEvent(event),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mWinModule(0),
	  mFlags(event.flags()),
	  mLateCancel(event.lateCancel()),
	  mErrorWindow(false),
	  mNoPostAction(alarm.type() & KAAlarm::REMINDER_ALARM),
	  mRecreating(false),
	  mBeep(event.beep()),
	  mSpeak(event.speak()),
	  mRescheduleEvent(reschedule_event),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	// Set to save settings automatically, but don't save window size.
	// File alarm window size is saved elsewhere.
	setAutoSaveSettings(QString::fromLatin1("MessageWin"), false);
	initView();
	mWindowList.append(this);
	if (event.autoClose())
		mCloseTime = alarm.dateTime().dateTime().addSecs(event.lateCancel() * 60);
}

/******************************************************************************
*  Construct the message window for a specified error message.
*/
MessageWin::MessageWin(const KAEvent& event, const DateTime& alarmDateTime, const QStringList& errmsgs)
	: MainWindowBase(0, "MessageWin", WFLAGS | Qt::WGroupLeader | Qt::WStyle_ContextHelp),
	  mMessage(event.cleanText()),
	  mDateTime(alarmDateTime),
	  mEventID(event.id()),
	  mAlarmType(KAAlarm::MAIN_ALARM),
	  mAction(event.action()),
	  mKMailSerialNumber(0),
	  mErrorMsgs(errmsgs),
	  mRestoreHeight(0),
	  mConfirmAck(false),
	  mShowEdit(false),
	  mNoDefer(true),
	  mInvalid(false),
	  mArtsDispatcher(0),
	  mPlayObject(0),
	  mEvent(event),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mWinModule(0),
	  mErrorWindow(true),
	  mNoPostAction(true),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(errmsg)" << endl;
	initView();
	mWindowList.append(this);
}

/******************************************************************************
*  Construct the message window for restoration by session management.
*  The window is initialised by readProperties().
*/
MessageWin::MessageWin()
	: MainWindowBase(0, "MessageWin", WFLAGS),
	  mArtsDispatcher(0),
	  mPlayObject(0),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mWinModule(0),
	  mErrorWindow(false),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(restore)\n";
	mWindowList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageWin::~MessageWin()
{
	kdDebug(5950) << "MessageWin::~MessageWin(" << mEventID << ")" << endl;
	stopPlay();
	delete mWinModule;
	mWinModule = 0;
	mErrorMessages.remove(mEventID);
	mWindowList.remove(this);
	if (!mRecreating)
	{
		if (!mNoPostAction  &&  !mEvent.postAction().isEmpty())
			theApp()->alarmCompleted(mEvent);
		if (!mWindowList.count())
			theApp()->quitIf();
	}
}

/******************************************************************************
*  Construct the message window.
*/
void MessageWin::initView()
{
	bool reminder = (!mErrorWindow  &&  (mAlarmType & KAAlarm::REMINDER_ALARM));
	int leading = fontMetrics().leading();
	setCaption((mAlarmType & KAAlarm::REMINDER_ALARM) ? i18n("Reminder") : i18n("Message"));
	QWidget* topWidget = new QWidget(this, "messageWinTop");
	setCentralWidget(topWidget);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget, KDialog::marginHint(), KDialog::spacingHint());

	if (mDateTime.isValid())
	{
		// Show the alarm date/time, together with an "Advance reminder" text where appropriate
		QFrame* frame = 0;
		QVBoxLayout* layout = topLayout;
		if (reminder)
		{
			frame = new QFrame(topWidget);
			frame->setFrameStyle(QFrame::Box | QFrame::Raised);
			topLayout->addWidget(frame, 0, Qt::AlignHCenter);
			layout = new QVBoxLayout(frame, leading + frame->frameWidth(), leading);
		}

		// Alarm date/time
		QLabel* label = new QLabel(frame ? frame : topWidget);
		label->setText(mDateTime.isDateOnly()
		               ? KGlobal::locale()->formatDate(mDateTime.date(), true)
		               : KGlobal::locale()->formatDateTime(mDateTime.dateTime()));
		if (!frame)
			label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignHCenter);
		QWhatsThis::add(label,
		      i18n("The scheduled date/time for the message (as opposed to the actual time of display)."));

		if (frame)
		{
			label = new QLabel(frame);
			label->setText(i18n("Reminder"));
			label->setFixedSize(label->sizeHint());
			layout->addWidget(label, 0, Qt::AlignHCenter);
			frame->setFixedSize(frame->sizeHint());
		}
	}

	if (!mErrorWindow)
	{
		// It's a normal alarm message window
		switch (mAction)
		{
			case KAEvent::FILE:
			{
				// Display the file name
				QLabel* label = new QLabel(mMessage, topWidget);
				label->setFrameStyle(QFrame::Box | QFrame::Raised);
				label->setFixedSize(label->sizeHint());
				QWhatsThis::add(label, i18n("The file whose contents are displayed below"));
				topLayout->addWidget(label, 0, Qt::AlignHCenter);

				// Display contents of file
				bool opened = false;
				bool dir = false;
				QString tmpFile;
				KURL url(mMessage);
				if (KIO::NetAccess::download(url, tmpFile, MainWindow::mainMainWindow()))
				{
					QFile qfile(tmpFile);
					QFileInfo info(qfile);
					if (!(dir = info.isDir()))
					{
						opened = true;
						KTextBrowser* view = new KTextBrowser(topWidget, "fileContents");
						MWMimeSourceFactory msf(tmpFile, view);
						view->setMinimumSize(view->sizeHint());
						topLayout->addWidget(view);

						// Set the default size to 20 lines square.
						// Note that after the first file has been displayed, this size
						// is overridden by the user-set default stored in the config file.
						// So there is no need to calculate an accurate size.
						int h = 20*view->fontMetrics().lineSpacing() + 2*view->frameWidth();
						view->resize(QSize(h, h).expandedTo(view->sizeHint()));
						QWhatsThis::add(view, i18n("The contents of the file to be displayed"));
					}
					KIO::NetAccess::removeTempFile(tmpFile);
				}
				if (!opened)
				{
					// File couldn't be opened
					bool exists = KIO::NetAccess::exists(url, true, MainWindow::mainMainWindow());
					mErrorMsgs += dir ? i18n("File is a folder") : exists ? i18n("Failed to open file") : i18n("File not found");
				}
				break;
			}
			case KAEvent::MESSAGE:
			{
				// Message label
				// Using MessageText instead of QLabel allows scrolling and mouse copying
				MessageText* text = new MessageText(mMessage, QString::null, topWidget);
				text->setFrameStyle(QFrame::NoFrame);
				text->setPaper(mBgColour);
				text->setPaletteForegroundColor(mFgColour);
				text->setFont(mFont);
				int lineSpacing = text->fontMetrics().lineSpacing();
				QSize s = text->sizeHint();
				int h = s.height();
				text->setMaximumHeight(h + text->scrollBarHeight());
				text->setMinimumHeight(QMIN(h, lineSpacing*4));
				text->setMaximumWidth(s.width() + text->scrollBarWidth());
				QWhatsThis::add(text, i18n("The alarm message"));
				int vspace = lineSpacing/2;
				int hspace = lineSpacing - KDialog::marginHint();
				topLayout->addSpacing(vspace);
				topLayout->addStretch();
				// Don't include any horizontal margins if message is 2/3 screen width
				if (!mWinModule)
					mWinModule = new KWinModule(0, KWinModule::INFO_DESKTOP);
				if (text->sizeHint().width() >= mWinModule->workArea().width()*2/3)
					topLayout->addWidget(text, 1, Qt::AlignHCenter);
				else
				{
					QBoxLayout* layout = new QHBoxLayout(topLayout);
					layout->addSpacing(hspace);
					layout->addWidget(text, 1, Qt::AlignHCenter);
					layout->addSpacing(hspace);
				}
				if (!reminder)
					topLayout->addStretch();
				break;
			}
			case KAEvent::COMMAND:
			case KAEvent::EMAIL:
			default:
				break;
		}

		if (reminder)
		{
			// Reminder: show remaining time until the actual alarm
			mRemainingText = new QLabel(topWidget);
			mRemainingText->setFrameStyle(QFrame::Box | QFrame::Raised);
			mRemainingText->setMargin(leading);
			if (mDateTime.isDateOnly()  ||  QDate::currentDate().daysTo(mDateTime.date()) > 0)
			{
				setRemainingTextDay();
				MidnightTimer::connect(this, SLOT(setRemainingTextDay()));    // update every day
			}
			else
			{
				setRemainingTextMinute();
				MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
			}
			topLayout->addWidget(mRemainingText, 0, Qt::AlignHCenter);
			topLayout->addSpacing(KDialog::spacingHint());
			topLayout->addStretch();
		}
	}
	else
	{
		// It's an error message
		switch (mAction)
		{
			case KAEvent::EMAIL:
			{
				// Display the email addresses and subject.
				QFrame* frame = new QFrame(topWidget);
				frame->setFrameStyle(QFrame::Box | QFrame::Raised);
				QWhatsThis::add(frame, i18n("The email to send"));
				topLayout->addWidget(frame, 0, Qt::AlignHCenter);
				QGridLayout* grid = new QGridLayout(frame, 2, 2, KDialog::marginHint(), KDialog::spacingHint());

				QLabel* label = new QLabel(i18n("Email addressee", "To:"), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 0, 0, Qt::AlignLeft);
				label = new QLabel(mEvent.emailAddresses("\n"), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 0, 1, Qt::AlignLeft);

				label = new QLabel(i18n("Email subject", "Subject:"), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 1, 0, Qt::AlignLeft);
				label = new QLabel(mEvent.emailSubject(), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 1, 1, Qt::AlignLeft);
				break;
			}
			case KAEvent::COMMAND:
			case KAEvent::FILE:
			case KAEvent::MESSAGE:
			default:
				// Just display the error message strings
				break;
		}
	}

	if (!mErrorMsgs.count())
		topWidget->setBackgroundColor(mBgColour);
	else
	{
		setCaption(i18n("Error"));
		QBoxLayout* layout = new QHBoxLayout(topLayout);
		layout->setMargin(2*KDialog::marginHint());
		layout->addStretch();
		QLabel* label = new QLabel(topWidget);
		label->setPixmap(DesktopIcon("error"));
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignRight);
		QBoxLayout* vlayout = new QVBoxLayout(layout);
		for (QStringList::Iterator it = mErrorMsgs.begin();  it != mErrorMsgs.end();  ++it)
		{
			label = new QLabel(*it, topWidget);
			label->setFixedSize(label->sizeHint());
			vlayout->addWidget(label, 0, Qt::AlignLeft);
		}
		layout->addStretch();
	}

	QGridLayout* grid = new QGridLayout(1, 4);
	topLayout->addLayout(grid);
	grid->setColStretch(0, 1);     // keep the buttons right-adjusted in the window
	int gridIndex = 1;

	// Close button
	mOkButton = new KPushButton(KStdGuiItem::close(), topWidget);
	// Prevent accidental acknowledgement of the message if the user is typing when the window appears
	mOkButton->clearFocus();
	mOkButton->setFocusPolicy(QWidget::ClickFocus);    // don't allow keyboard selection
	mOkButton->setFixedSize(mOkButton->sizeHint());
	connect(mOkButton, SIGNAL(clicked()), SLOT(close()));
	grid->addWidget(mOkButton, 0, gridIndex++, AlignHCenter);
	QWhatsThis::add(mOkButton, i18n("Acknowledge the alarm"));

	if (mShowEdit)
	{
		// Edit button
		mEditButton = new QPushButton(i18n("&Edit..."), topWidget);
		mEditButton->setFocusPolicy(QWidget::ClickFocus);    // don't allow keyboard selection
		mEditButton->setFixedSize(mEditButton->sizeHint());
		connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
		grid->addWidget(mEditButton, 0, gridIndex++, AlignHCenter);
		QWhatsThis::add(mEditButton, i18n("Edit the alarm."));
	}

	if (!mNoDefer)
	{
		// Defer button
		mDeferButton = new QPushButton(i18n("&Defer..."), topWidget);
		mDeferButton->setFocusPolicy(QWidget::ClickFocus);    // don't allow keyboard selection
		mDeferButton->setFixedSize(mDeferButton->sizeHint());
		connect(mDeferButton, SIGNAL(clicked()), SLOT(slotDefer()));
		grid->addWidget(mDeferButton, 0, gridIndex++, AlignHCenter);
		QWhatsThis::add(mDeferButton,
		      i18n("Defer the alarm until later.\n"
		           "You will be prompted to specify when the alarm should be redisplayed."));

		setDeferralLimit(mEvent);    // ensure that button is disabled when alarm can't be deferred any more
	}

#ifndef WITHOUT_ARTS
	if (!mAudioFile.isEmpty()  &&  (mVolume || mFadeVolume > 0))
	{
		// Silence button to stop sound repetition
		QPixmap pixmap = MainBarIcon("player_stop");
		mSilenceButton = new QPushButton(topWidget);
		mSilenceButton->setPixmap(pixmap);
		mSilenceButton->setFixedSize(mSilenceButton->sizeHint());
		connect(mSilenceButton, SIGNAL(clicked()), SLOT(stopPlay()));
		grid->addWidget(mSilenceButton, 0, gridIndex++, AlignHCenter);
		QToolTip::add(mSilenceButton, i18n("Stop sound"));
		QWhatsThis::add(mSilenceButton, i18n("Stop playing the sound"));
		// To avoid getting in a mess, disable the button until sound playing has been set up
		mSilenceButton->setEnabled(false);
	}
#endif

	KIconLoader iconLoader;
	if (mKMailSerialNumber)
	{
		// KMail button
		QPixmap pixmap = iconLoader.loadIcon(QString::fromLatin1("kmail"), KIcon::MainToolbar);
		mKMailButton = new QPushButton(topWidget);
		mKMailButton->setPixmap(pixmap);
		mKMailButton->setFixedSize(mKMailButton->sizeHint());
		connect(mKMailButton, SIGNAL(clicked()), SLOT(slotShowKMailMessage()));
		grid->addWidget(mKMailButton, 0, gridIndex++, AlignHCenter);
		QToolTip::add(mKMailButton, i18n("Locate this email in KMail", "Locate in KMail"));
		QWhatsThis::add(mKMailButton, i18n("Locate and highlight this email in KMail"));
	}
	else
		mKMailButton = 0;

	// KAlarm button
	QPixmap pixmap = iconLoader.loadIcon(QString::fromLatin1(kapp->aboutData()->appName()), KIcon::MainToolbar);
	mKAlarmButton = new QPushButton(topWidget);
	mKAlarmButton->setPixmap(pixmap);
	mKAlarmButton->setFixedSize(mKAlarmButton->sizeHint());
	connect(mKAlarmButton, SIGNAL(clicked()), SLOT(displayMainWindow()));
	grid->addWidget(mKAlarmButton, 0, gridIndex++, AlignHCenter);
	QString actKAlarm = i18n("Activate KAlarm");
	QToolTip::add(mKAlarmButton, actKAlarm);
	QWhatsThis::add(mKAlarmButton, actKAlarm);

	// Disable all buttons initially, to prevent accidental clicking on if they happen to be
	// under the mouse just as the window appears.
	mOkButton->setEnabled(false);
	if (mDeferButton)
		mDeferButton->setEnabled(false);
	if (mEditButton)
		mEditButton->setEnabled(false);
	if (mKMailButton)
		mKMailButton->setEnabled(false);
	mKAlarmButton->setEnabled(false);

	topLayout->activate();
	setMinimumSize(QSize(grid->sizeHint().width() + 2*KDialog::marginHint(), sizeHint().height()));

	WId winid = winId();
	unsigned long wstate = (Preferences::modalMessages() ? NET::Modal : 0) | NET::Sticky | NET::StaysOnTop;
	KWin::setState(winid, wstate);
	KWin::setOnAllDesktops(winid, true);
}

/******************************************************************************
* Set the remaining time text in a reminder window.
* Called at the start of every day (at the user-defined start-of-day time).
*/
void MessageWin::setRemainingTextDay()
{
	QString text;
	int days = QDate::currentDate().daysTo(mDateTime.date());
	if (days <= 0  &&  !mDateTime.isDateOnly())
	{
		// The alarm is due today, so start refreshing every minute
		MidnightTimer::disconnect(this, SLOT(setRemainingTextDay()));
		setRemainingTextMinute();
		MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
	}
	else
	{
		if (days <= 0)
			text = i18n("Today");
		else if (days % 7)
			text = i18n("Tomorrow", "in %n days' time", days);
		else
			text = i18n("in 1 week's time", "in %n weeks' time", days/7);
	}
	mRemainingText->setText(text);
}

/******************************************************************************
* Set the remaining time text in a reminder window.
* Called on every minute boundary.
*/
void MessageWin::setRemainingTextMinute()
{
	QString text;
	int mins = (QDateTime::currentDateTime().secsTo(mDateTime.dateTime()) + 59) / 60;
	if (mins < 60)
		text = i18n("in 1 minute's time", "in %n minutes' time", (mins > 0 ? mins : 0));
	else if (mins % 60 == 0)
		text = i18n("in 1 hour's time", "in %n hours' time", mins/60);
	else if (mins % 60 == 1)
		text = i18n("in 1 hour 1 minute's time", "in %n hours 1 minute's time", mins/60);
	else
		text = i18n("in 1 hour %1 minutes' time", "in %n hours %1 minutes' time", mins/60).arg(mins%60);
	mRemainingText->setText(text);
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWin::saveProperties(KConfig* config)
{
	if (mShown  &&  !mErrorWindow)
	{
		config->writeEntry(QString::fromLatin1("EventID"), mEventID);
		config->writeEntry(QString::fromLatin1("AlarmType"), mAlarmType);
		config->writeEntry(QString::fromLatin1("Message"), mMessage);
		config->writeEntry(QString::fromLatin1("Type"), mAction);
		config->writeEntry(QString::fromLatin1("Font"), mFont);
		config->writeEntry(QString::fromLatin1("BgColour"), mBgColour);
		config->writeEntry(QString::fromLatin1("FgColour"), mFgColour);
		config->writeEntry(QString::fromLatin1("ConfirmAck"), mConfirmAck);
		if (mDateTime.isValid())
		{
			config->writeEntry(QString::fromLatin1("Time"), mDateTime.dateTime());
			config->writeEntry(QString::fromLatin1("DateOnly"), mDateTime.isDateOnly());
		}
		if (mCloseTime.isValid())
			config->writeEntry(QString::fromLatin1("Expiry"), mCloseTime);
#ifndef WITHOUT_ARTS
		if (mAudioRepeat  &&  mSilenceButton  &&  mSilenceButton->isEnabled())
		{
			// Only need to restart sound file playing if it's being repeated
			config->writePathEntry(QString::fromLatin1("AudioFile"), mAudioFile);
			config->writeEntry(QString::fromLatin1("Volume"), static_cast<int>(mVolume * 100));
		}
#endif
		config->writeEntry(QString::fromLatin1("Speak"), mSpeak);
		config->writeEntry(QString::fromLatin1("Height"), height());
		config->writeEntry(QString::fromLatin1("DeferMins"), mDefaultDeferMinutes);
		config->writeEntry(QString::fromLatin1("NoDefer"), mNoDefer);
		config->writeEntry(QString::fromLatin1("NoPostAction"), mNoPostAction);
		config->writeEntry(QString::fromLatin1("KMailSerial"), mKMailSerialNumber);
	}
	else
		config->writeEntry(QString::fromLatin1("Invalid"), true);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(KConfig* config)
{
	mInvalid             = config->readBoolEntry(QString::fromLatin1("Invalid"), false);
	mEventID             = config->readEntry(QString::fromLatin1("EventID"));
	mAlarmType           = KAAlarm::Type(config->readNumEntry(QString::fromLatin1("AlarmType")));
	mMessage             = config->readEntry(QString::fromLatin1("Message"));
	mAction              = KAEvent::Action(config->readNumEntry(QString::fromLatin1("Type")));
	mFont                = config->readFontEntry(QString::fromLatin1("Font"));
	mBgColour            = config->readColorEntry(QString::fromLatin1("BgColour"));
	mFgColour            = config->readColorEntry(QString::fromLatin1("FgColour"));
	mConfirmAck          = config->readBoolEntry(QString::fromLatin1("ConfirmAck"));
	QDateTime invalidDateTime;
	QDateTime dt         = config->readDateTimeEntry(QString::fromLatin1("Time"), &invalidDateTime);
	bool dateOnly        = config->readBoolEntry(QString::fromLatin1("DateOnly"));
	mDateTime.set(dt, dateOnly);
	mCloseTime           = config->readDateTimeEntry(QString::fromLatin1("Expiry"), &invalidDateTime);
#ifndef WITHOUT_ARTS
	mAudioFile           = config->readPathEntry(QString::fromLatin1("AudioFile"));
	mVolume              = static_cast<float>(config->readNumEntry(QString::fromLatin1("Volume"))) / 100;
	mFadeVolume          = -1;
	mFadeSeconds         = 0;
	if (!mAudioFile.isEmpty())
		mAudioRepeat = true;
#endif
	mSpeak               = config->readBoolEntry(QString::fromLatin1("Speak"));
	mRestoreHeight       = config->readNumEntry(QString::fromLatin1("Height"));
	mDefaultDeferMinutes = config->readNumEntry(QString::fromLatin1("DeferMins"));
	mNoDefer             = config->readBoolEntry(QString::fromLatin1("NoDefer"));
	mNoPostAction        = config->readBoolEntry(QString::fromLatin1("NoPostAction"));
	mKMailSerialNumber   = config->readUnsignedLongNumEntry(QString::fromLatin1("KMailSerial"));
	mShowEdit            = false;
	kdDebug(5950) << "MessageWin::readProperties(" << mEventID << ")" << endl;
	if (mAlarmType != KAAlarm::INVALID_ALARM)
	{
		// Recreate the event from the calendar file (if possible)
		if (!mEventID.isEmpty())
		{
			const Event* kcalEvent = AlarmCalendar::activeCalendar()->event(mEventID);
			if (!kcalEvent)
			{
				// It's not in the active calendar, so try the displaying calendar
				AlarmCalendar* cal = AlarmCalendar::displayCalendar();
				if (cal->isOpen())
					kcalEvent = cal->event(KAEvent::uid(mEventID, KAEvent::DISPLAYING));
			}
			if (kcalEvent)
			{
				mEvent.set(*kcalEvent);
				mEvent.setUid(KAEvent::ACTIVE);    // in case it came from the display calendar
				mShowEdit = true;
			}
		}
		initView();
	}
}

/******************************************************************************
*  Returns the existing message window (if any) which is displaying the event
*  with the specified ID.
*/
MessageWin* MessageWin::findEvent(const QString& eventID)
{
	for (QValueList<MessageWin*>::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
	{
		MessageWin* w = *it;
		if (w->mEventID == eventID  &&  !w->mErrorWindow)
			return w;
	}
	return 0;
}

/******************************************************************************
*  Beep and play the audio file, as appropriate.
*/
void MessageWin::playAudio()
{
	if (mBeep)
	{
		// Beep using two methods, in case the sound card/speakers are switched off or not working
		KNotifyClient::beep();     // beep through the sound card & speakers
		QApplication::beep();      // beep through the internal speaker
	}
	if (!mAudioFile.isEmpty())
	{
		if (!mVolume  &&  mFadeVolume <= 0)
			return;    // ensure zero volume doesn't play anything
#ifdef WITHOUT_ARTS
		QString play = mAudioFile;
		QString file = QString::fromLatin1("file:");
		if (mAudioFile.startsWith(file))
			play = mAudioFile.mid(file.length());
		KAudioPlayer::play(QFile::encodeName(play));
#else
		// An audio file is specified. Because loading it may take some time,
		// call it on a timer to allow the window to display first.
		QTimer::singleShot(0, this, SLOT(slotPlayAudio()));
#endif
	}
	else if (mSpeak)
	{
		// The message is to be spoken. In case of error messges,
		// call it on a timer to allow the window to display first.
		QTimer::singleShot(0, this, SLOT(slotSpeak()));
	}
}

/******************************************************************************
*  Speak the message.
*  Called asynchronously to avoid delaying the display of the message.
*/
void MessageWin::slotSpeak()
{
	DCOPClient* client = kapp->dcopClient();
	if (!client->isApplicationRegistered("kttsd"))
	{
		// kttsd is not running, so start it
		QString error;
		if (kapp->startServiceByDesktopName("kttsd", QStringList(), &error))
		{
			kdDebug(5950) << "MessageWin::slotSpeak(): failed to start kttsd: " << error << endl;
			if (!haveErrorMessage(ErrMsg_Speak))
			{
				KMessageBox::detailedError(0, i18n("Unable to speak message"), error);
				clearErrorMessage(ErrMsg_Speak);
			}
			return;
		}
	}
	QByteArray  data;
	QDataStream arg(data, IO_WriteOnly);
	arg << mMessage << "";
	if (!client->send("kttsd", "KSpeech", "sayMessage(QString,QString)", data))
	{
		kdDebug(5950) << "MessageWin::slotSpeak(): sayMessage() DCOP error" << endl;
		if (!haveErrorMessage(ErrMsg_Speak))
		{
			KMessageBox::detailedError(0, i18n("Unable to speak message"), i18n("DCOP Call sayMessage failed"));
			clearErrorMessage(ErrMsg_Speak);
		}
	}
}

/******************************************************************************
*  Play the audio file.
*  Called asynchronously to avoid delaying the display of the message.
*/
void MessageWin::slotPlayAudio()
{
#ifndef WITHOUT_ARTS
	// First check that it exists, to avoid possible crashes if the filename is badly specified
	MainWindow* mmw = MainWindow::mainMainWindow();
	KURL url(mAudioFile);
	if (!url.isValid()  ||  !KIO::NetAccess::exists(url, true, mmw)
	||  !KIO::NetAccess::download(url, mLocalAudioFile, mmw))
	{
		kdError(5950) << "MessageWin::playAudio(): Open failure: " << mAudioFile << endl;
		if (!haveErrorMessage(ErrMsg_AudioFile))
		{
			KMessageBox::error(this, i18n("Cannot open audio file:\n%1").arg(mAudioFile));
			clearErrorMessage(ErrMsg_AudioFile);
		}
		return;
	}
	if (!mArtsDispatcher)
	{
		mFadeTimer = 0;
		mPlayTimer = new QTimer(this);
		connect(mPlayTimer, SIGNAL(timeout()), SLOT(checkAudioPlay()));
		mArtsDispatcher = new KArtsDispatcher;
		mPlayedOnce = false;
		mAudioFileStart = QTime::currentTime();
		initAudio(true);
		if (!mPlayObject->object().isNull())
			checkAudioPlay();
#if KDE_VERSION >= 308
		if (!mUsingKMix  &&  mVolume >= 0)
		{
			// Output error message now that everything else has been done.
			// (Outputting it earlier would delay things until it is acknowledged.)
			kdWarning(5950) << "Unable to set master volume (KMix: " << mKMixError << ")\n";
			if (!haveErrorMessage(ErrMsg_Volume))
			{
				KMessageBox::information(this, i18n("Unable to set master volume\n(Error accessing KMix:\n%1)").arg(mKMixError),
				                         QString::null, QString::fromLatin1("KMixError"));
				clearErrorMessage(ErrMsg_Volume);
			}
		}
#endif
	}
#endif
}

#ifndef WITHOUT_ARTS
/******************************************************************************
*  Set up the audio file for playing.
*/
void MessageWin::initAudio(bool firstTime)
{
	KArtsServer aserver;
	Arts::SoundServerV2 sserver = aserver.server();
	KDE::PlayObjectFactory factory(sserver);
	mPlayObject = factory.createPlayObject(mLocalAudioFile, true);
	if (firstTime)
	{
		// Save the existing sound volume setting for restoration afterwards,
		// and set the desired volume for the alarm.
		mUsingKMix = false;
		float volume = mVolume;    // initial volume
		if (volume >= 0)
		{
			// The volume has been specified
			if (mFadeVolume >= 0)
				volume = mFadeVolume;    // fading, so adjust the initial volume

			// Get the current master volume from KMix
			int vol = getKMixVolume();
			if (vol >= 0)
			{
				mOldVolume = vol;    // success
				mUsingKMix = true;
				setKMixVolume(static_cast<int>(volume * 100));
			}
		}
		if (!mUsingKMix)
		{
			/* Adjust within the current master volume, because either
			 * a) the volume is not specified, in which case we want to play
			 *    at 100% of the current master volume setting, or
			 * b) KMix is not available to set the master volume.
			 */
			mOldVolume = sserver.outVolume().scaleFactor();    // save volume for restoration afterwards
			sserver.outVolume().scaleFactor(volume >= 0 ? volume : 1);
		}
	}
	mSilenceButton->setEnabled(true);
	mPlayed = false;
	connect(mPlayObject, SIGNAL(playObjectCreated()), SLOT(checkAudioPlay()));
	if (!mPlayObject->object().isNull())
		checkAudioPlay();
}
#endif

/******************************************************************************
*  Called when the audio file has loaded and is ready to play, or on a timer
*  when play is expected to have completed.
*  If it is ready to play, start playing it (for the first time or repeated).
*  If play has not yet completed, wait a bit longer.
*/
void MessageWin::checkAudioPlay()
{
#ifndef WITHOUT_ARTS
	if (!mPlayObject)
		return;
	if (mPlayObject->state() == Arts::posIdle)
	{
		// The file has loaded and is ready to play, or play has completed
		if (mPlayedOnce  &&  !mAudioRepeat)
		{
			// Play has completed
			stopPlay();
			return;
		}

		// Start playing the file, either for the first time or again
		kdDebug(5950) << "MessageWin::checkAudioPlay(): start\n";
		if (!mPlayedOnce)
		{
			// Start playing the file for the first time
			QTime now = QTime::currentTime();
			mAudioFileLoadSecs = mAudioFileStart.secsTo(now);
			if (mAudioFileLoadSecs < 0)
				mAudioFileLoadSecs += 86400;
			if (mVolume >= 0  &&  mFadeVolume >= 0  &&  mFadeSeconds > 0)
			{
				// Set up volume fade
				mAudioFileStart = now;
				mFadeTimer = new QTimer(this);
				connect(mFadeTimer, SIGNAL(timeout()), SLOT(slotFade()));
				mFadeTimer->start(1000);     // adjust volume every second
			}
			mPlayedOnce = true;
		}
		if (mAudioFileLoadSecs < 3)
		{
			/* The aRts library takes several attempts before a PlayObject can
			 * be replayed, leaving a gap of perhaps 5 seconds between plays.
			 * So if loading the file takes a short time, it's better to reload
			 * the PlayObject rather than try to replay the same PlayObject.
			 */
			if (mPlayed)
			{
				// Playing has completed. Start playing again.
				delete mPlayObject;
				initAudio(false);
				if (mPlayObject->object().isNull())
					return;
			}
			mPlayed = true;
			mPlayObject->play();
		}
		else
		{
			// The file is slow to load, so attempt to replay the PlayObject
			static Arts::poTime t0((long)0, (long)0, 0, std::string());
			Arts::poTime current = mPlayObject->currentTime();
			if (current.seconds || current.ms)
				mPlayObject->seek(t0);
			else
				mPlayObject->play();
		}
	}

	// The sound file is still playing
	Arts::poTime overall = mPlayObject->overallTime();
	Arts::poTime current = mPlayObject->currentTime();
	int time = 1000*(overall.seconds - current.seconds) + overall.ms - current.ms;
	if (time < 0)
		time = 0;
	kdDebug(5950) << "MessageWin::checkAudioPlay(): wait for " << (time+100) << "ms\n";
	mPlayTimer->start(time + 100, true);
#endif
}

/******************************************************************************
*  Called when play completes, the Silence button is clicked, or the window is
*  closed, to reset the sound volume and terminate audio access.
*/
void MessageWin::stopPlay()
{
#ifndef WITHOUT_ARTS
	if (mArtsDispatcher)
	{
		// Restore the sound volume to what it was before the sound file
		// was played, provided that nothing else has modified it since.
		if (!mUsingKMix)
		{
			KArtsServer aserver;
			Arts::StereoVolumeControl svc = aserver.server().outVolume();
			float currentVolume = svc.scaleFactor();
			float eventVolume = mVolume;
			if (eventVolume < 0)
				eventVolume = 1;
			if (currentVolume == eventVolume)
				svc.scaleFactor(mOldVolume);
		}
		else if (mVolume >= 0)
		{
			int eventVolume = static_cast<int>(mVolume * 100);
			int currentVolume = getKMixVolume();
			// Volume returned isn't always exactly equal to volume set
			if (currentVolume < 0  ||  abs(currentVolume - eventVolume) < 5)
				setKMixVolume(static_cast<int>(mOldVolume));
		}
	}
	delete mPlayObject;      mPlayObject = 0;
	delete mArtsDispatcher;  mArtsDispatcher = 0;
	if (!mLocalAudioFile.isEmpty())
	{
		KIO::NetAccess::removeTempFile(mLocalAudioFile);   // removes it only if it IS a temporary file
		mLocalAudioFile = QString::null;
	}
	if (mSilenceButton)
		mSilenceButton->setEnabled(false);
#endif
}

/******************************************************************************
*  Called every second to fade the volume when the audio file starts playing.
*/
void MessageWin::slotFade()
{
#ifndef WITHOUT_ARTS
	QTime now = QTime::currentTime();
	int elapsed = mAudioFileStart.secsTo(now);
	if (elapsed < 0)
		elapsed += 86400;    // it's the next day
	float volume;
	if (elapsed >= mFadeSeconds)
	{
		// The fade has finished. Set to normal volume.
		volume = mVolume;
		delete mFadeTimer;
		mFadeTimer = 0;
		if (!mVolume)
		{
			kdDebug(5950) << "MessageWin::slotFade(0)\n";
			stopPlay();
			return;
		}
	}
	else
		volume = mFadeVolume  +  ((mVolume - mFadeVolume) * elapsed) / mFadeSeconds;
	kdDebug(5950) << "MessageWin::slotFade(" << volume << ")\n";
	if (mArtsDispatcher)
	{
		if (mUsingKMix)
			setKMixVolume(static_cast<int>(volume * 100));
		else if (mArtsDispatcher)
		{
			KArtsServer aserver;
			aserver.server().outVolume().scaleFactor(volume);
		}
	}
#endif
}

#ifndef WITHOUT_ARTS
/******************************************************************************
*  Get the master volume from KMix.
*  Reply < 0 if failure.
*/
int MessageWin::getKMixVolume()
{
	if (!KAlarm::runProgram(KMIX_APP_NAME, KMIX_DCOP_WINDOW, mKMixName, mKMixError))   // start KMix if it isn't already running
		return -1;
	QByteArray  data, replyData;
	QCString    replyType;
	QDataStream arg(data, IO_WriteOnly);
	if (!kapp->dcopClient()->call(mKMixName, KMIX_DCOP_OBJECT, "masterVolume()", data, replyType, replyData)
	||  replyType != "int")
		return -1;
	int result;
	QDataStream reply(replyData, IO_ReadOnly);
	reply >> result;
	return (result >= 0) ? result : 0;
}

/******************************************************************************
*  Set the master volume using KMix.
*/
void MessageWin::setKMixVolume(int percent)
{
	if (!mUsingKMix)
		return;
	if (!KAlarm::runProgram(KMIX_APP_NAME, KMIX_DCOP_WINDOW, mKMixName, mKMixError))   // start KMix if it isn't already running
		return;
	QByteArray  data;
	QDataStream arg(data, IO_WriteOnly);
	arg << percent;
	if (!kapp->dcopClient()->send(mKMixName, KMIX_DCOP_OBJECT, "setMasterVolume(int)", data))
		kdError(5950) << "MessageWin::setKMixVolume(): kmix dcop call failed\n";
}
#endif

/******************************************************************************
*  Raise the alarm window, re-output any required audio notification, and
*  reschedule the alarm in the calendar file.
*/
void MessageWin::repeat(const KAAlarm& alarm)
{
	if (mDeferDlg)
	{
		// Cancel any deferral dialogue so that the user notices something's going on,
		// and also because the deferral time limit will have changed.
		delete mDeferDlg;
		mDeferDlg = 0;
	}
	const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::activeCalendar()->event(mEventID);
	if (kcalEvent)
	{
		mAlarmType = alarm.type();    // store new alarm type for use if it is later deferred
		if (!mDeferDlg  ||  Preferences::modalMessages())
		{
			raise();
			playAudio();
		}
		KAEvent event(*kcalEvent);
		mDeferButton->setEnabled(true);
		setDeferralLimit(event);    // ensure that button is disabled when alarm can't be deferred any more
		theApp()->alarmShowing(event, mAlarmType, mDateTime);
	}
}

/******************************************************************************
*  Display the window.
*  If windows are being positioned away from the mouse cursor, it is initially
*  positioned at the top left to slightly reduce the number of times the
*  windows need to be moved in showEvent().
*/
void MessageWin::show()
{
	if (mCloseTime.isValid())
	{
		// Set a timer to auto-close the window
		int delay = QDateTime::currentDateTime().secsTo(mCloseTime);
		if (delay < 0)
			delay = 0;
		QTimer::singleShot(delay * 1000, this, SLOT(close()));
		if (!delay)
			return;    // don't show the window if auto-closing is already due
	}
	if (Preferences::messageButtonDelay() == 0)
		move(0, 0);
	MainWindowBase::show();
}

/******************************************************************************
*  Returns the window's recommended size exclusive of its frame.
*  For message windows, the size if limited to fit inside the working area of
*  the desktop.
*/
QSize MessageWin::sizeHint() const
{
	if (mAction != KAEvent::MESSAGE)
		return MainWindowBase::sizeHint();
	if (!mWinModule)
		mWinModule = new KWinModule(0, KWinModule::INFO_DESKTOP);
	QSize frame = frameGeometry().size();
	QSize contents = geometry().size();
	QSize desktop  = mWinModule->workArea().size();
	QSize maxSize(desktop.width() - (frame.width() - contents.width()),
	              desktop.height() - (frame.height() - contents.height()));
	return MainWindowBase::sizeHint().boundedTo(maxSize);
}

/******************************************************************************
*  Called when the window is shown.
*  The first time, output any required audio notification, and reschedule or
*  delete the event from the calendar file.
*/
void MessageWin::showEvent(QShowEvent* se)
{
	MainWindowBase::showEvent(se);
	if (!mShown)
	{
		if (mErrorWindow)
			enableButtons();    // don't bother repositioning error messages
		else
		{
			/* Set the window size.
			 * Note that the frame thickness is not yet known when this
			 * method is called, so for large windows the size needs to be
			 * set again later.
			 */
			QSize s = sizeHint();     // fit the window round the message
			if (mAction == KAEvent::FILE  &&  !mErrorMsgs.count())
				KAlarm::readConfigWindowSize("FileMessage", s);
			resize(s);

			mButtonDelay = Preferences::messageButtonDelay() * 1000;
			if (!mButtonDelay)
			{
				/* Try to ensure that the window can't accidentally be acknowledged
				 * by the user clicking the mouse just as it appears.
				 * To achieve this, move the window so that the OK button is as far away
				 * from the cursor as possible. If the buttons are still too close to the
				 * cursor, disable the buttons for a short time.
				 * N.B. This can't be done in show(), since the geometry of the window
				 *      is not known until it is displayed. Unfortunately by moving the
				 *      window in showEvent(), a flicker is unavoidable.
				 *      See the Qt documentation on window geometry for more details.
				 */
				// PROBLEM: The frame size is not known yet!

				/* Find the usable area of the desktop or, if the desktop comprises
				 * multiple screens, the usable area of the current screen. (If the
				 * message is displayed on a screen other than that currently being
				 * worked with, it might not be noticed.)
				 */
				QPoint cursor = QCursor::pos();
				if (!mWinModule)
					mWinModule = new KWinModule(0, KWinModule::INFO_DESKTOP);
				QRect desk = mWinModule->workArea();
				QDesktopWidget* dw = QApplication::desktop();
				if (dw->numScreens() > 1)
					desk &= dw->screenGeometry(dw->screenNumber(cursor));

				QRect frame = frameGeometry();
				QRect rect  = geometry();
				// Find the offsets from the outside of the frame to the edges of the OK button
				QRect button(mOkButton->mapToParent(QPoint(0, 0)), mOkButton->mapToParent(mOkButton->rect().bottomRight()));
				int buttonLeft   = button.left() + rect.left() - frame.left();
				int buttonRight  = width() - button.right() + frame.right() - rect.right();
				int buttonTop    = button.top() + rect.top() - frame.top();
				int buttonBottom = height() - button.bottom() + frame.bottom() - rect.bottom();

				int centrex = (desk.width() + buttonLeft - buttonRight) / 2;
				int centrey = (desk.height() + buttonTop - buttonBottom) / 2;
				int x = (cursor.x() < centrex) ? desk.right() - frame.width() : desk.left();
				int y = (cursor.y() < centrey) ? desk.bottom() - frame.height() : desk.top();

				// Find the enclosing rectangle for the new button positions
				// and check if the cursor is too near
				QRect buttons = mOkButton->geometry().unite(mKAlarmButton->geometry());
				buttons.moveBy(rect.left() + x - frame.left(), rect.top() + y - frame.top());
				int minDistance = proximityMultiple * mOkButton->height();
				if ((abs(cursor.x() - buttons.left()) < minDistance
				  || abs(cursor.x() - buttons.right()) < minDistance)
				&&  (abs(cursor.y() - buttons.top()) < minDistance
				  || abs(cursor.y() - buttons.bottom()) < minDistance))
					mButtonDelay = proximityButtonDelay;    // too near - disable buttons initially

				if (x != frame.left()  ||  y != frame.top())
				{
					mPositioning = true;
					move(x, y);
				}
			}
			if (!mPositioning)
				displayComplete();    // play audio, etc.
			if (mAction == KAEvent::MESSAGE)
			{
				// Set the window size once the frame size is known
				QTimer::singleShot(0, this, SLOT(setMaxSize()));
			}
		}
		mShown = true;
	}
}

/******************************************************************************
*  Called when the window has been moved.
*/
void MessageWin::moveEvent(QMoveEvent* e)
{
	MainWindowBase::moveEvent(e);
	if (mPositioning)
	{
		// The window has just been initially positioned
		mPositioning = false;
		displayComplete();    // play audio, etc.
	}
}

/******************************************************************************
*  Reset the iniital window size if it exceeds the working area of the desktop.
*/
void MessageWin::setMaxSize()
{
	QSize s = sizeHint();
	if (width() > s.width()  ||  height() > s.height())
		resize(s);
}

/******************************************************************************
*  Called when the window has been displayed properly (in its correct position),
*  to play sounds and reschedule the event.
*/
void MessageWin::displayComplete()
{
	playAudio();
	if (mRescheduleEvent)
		theApp()->alarmShowing(mEvent, mAlarmType, mDateTime);

	// Enable the window's buttons either now or after the configured delay
	if (mButtonDelay > 0)
		QTimer::singleShot(mButtonDelay, this, SLOT(enableButtons()));
	else
		enableButtons();
}

/******************************************************************************
*  Enable the window's buttons.
*/
void MessageWin::enableButtons()
{
	mOkButton->setEnabled(true);
	mKAlarmButton->setEnabled(true);
	if (mDeferButton  &&  !mDisableDeferral)
		mDeferButton->setEnabled(true);
	if (mEditButton)
		mEditButton->setEnabled(true);
	if (mKMailButton)
		mKMailButton->setEnabled(true);
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*/
void MessageWin::resizeEvent(QResizeEvent* re)
{
	if (mRestoreHeight)
	{
		// Restore the window height on session restoration
		if (mRestoreHeight != re->size().height())
		{
			QSize size = re->size();
			size.setHeight(mRestoreHeight);
			resize(size);
		}
		else if (isVisible())
			mRestoreHeight = 0;
	}
	else
	{
		if (mShown  &&  mAction == KAEvent::FILE  &&  !mErrorMsgs.count())
			KAlarm::writeConfigWindowSize("FileMessage", re->size());
		MainWindowBase::resizeEvent(re);
	}
}

/******************************************************************************
*  Called when a close event is received.
*  Only quits the application if there is no system tray icon displayed.
*/
void MessageWin::closeEvent(QCloseEvent* ce)
{
	// Don't prompt or delete the alarm from the display calendar if the session is closing
	if (!theApp()->sessionClosingDown())
	{
		if (mConfirmAck  &&  !mNoCloseConfirm)
		{
			// Ask for confirmation of acknowledgement. Use warningYesNo() because its default is No.
			if (KMessageBox::warningYesNo(this, i18n("Do you really want to acknowledge this alarm?"),
			                                    i18n("Acknowledge Alarm"), i18n("&Acknowledge"), KStdGuiItem::cancel())
			    != KMessageBox::Yes)
			{
				ce->ignore();
				return;
			}
		}
		if (!mEventID.isNull())
		{
			// Delete from the display calendar
			KAlarm::deleteDisplayEvent(KAEvent::uid(mEventID, KAEvent::DISPLAYING));
		}
	}
	MainWindowBase::closeEvent(ce);
}

/******************************************************************************
*  Called when the KMail button is clicked.
*  Tells KMail to display the email message displayed in this message window.
*/
void MessageWin::slotShowKMailMessage()
{
	kdDebug(5950) << "MessageWin::slotShowKMailMessage()\n";
	if (!mKMailSerialNumber)
		return;
	QString err = KAlarm::runKMail(false);
	if (!err.isNull())
	{
		KMessageBox::sorry(this, err);
		return;
	}
	QCString    replyType;
	QByteArray  data, replyData;
	QDataStream arg(data, IO_WriteOnly);
	arg << (Q_UINT32)mKMailSerialNumber << QString::null;
	if (kapp->dcopClient()->call("kmail", KMAIL_DCOP_OBJECT, "showMail(Q_UINT32,QString)", data, replyType, replyData)
	&&  replyType == "bool")
	{
		bool result;
		QDataStream replyStream(replyData, IO_ReadOnly);
		replyStream >> result;
		if (result)
			return;    // success
	}
	kdError(5950) << "MessageWin::slotShowKMailMessage(): kmail dcop call failed\n";
	KMessageBox::sorry(this, i18n("Unable to locate this email in KMail"));
}

/******************************************************************************
*  Called when the Edit... button is clicked.
*  Displays the alarm edit dialog.
*/
void MessageWin::slotEdit()
{
	kdDebug(5950) << "MessageWin::slotEdit()" << endl;
	EditAlarmDlg editDlg(false, i18n("Edit Alarm"), this, "editDlg", &mEvent);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		editDlg.getEvent(event);

		// Update the displayed lists and the calendar file
		KAlarm::UpdateStatus status;
		if (AlarmCalendar::activeCalendar()->event(mEventID))
		{
			// The old alarm hasn't expired yet, so replace it
			status = KAlarm::modifyEvent(mEvent, event, 0, &editDlg);
			Undo::saveEdit(mEvent, event);
		}
		else
		{
			// The old event has expired, so simply create a new one
			status = KAlarm::addEvent(event, 0, &editDlg);
			Undo::saveAdd(event);
		}

		if (status == KAlarm::UPDATE_KORG_ERR)
			KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::KORG_ERR_MODIFY, 1);
		KAlarm::outputAlarmWarnings(&editDlg, &event);

		// Close the alarm window
		mNoCloseConfirm = true;   // allow window to close without confirmation prompt
		close();
	}
}

/******************************************************************************
* Set up to disable the defer button when the deferral limit is reached.
*/
void MessageWin::setDeferralLimit(const KAEvent& event)
{
	if (mDeferButton)
	{
		mDeferLimit = event.deferralLimit().dateTime();
		MidnightTimer::connect(this, SLOT(checkDeferralLimit()));   // check every day
		mDisableDeferral = false;
		checkDeferralLimit();
	}
}

/******************************************************************************
* Check whether the deferral limit has been reached.
* If so, disable the Defer button.
* N.B. Ideally, just a single QTimer::singleShot() call would be made to disable
*      the defer button at the corret time. But for a 32-bit integer, the
*      milliseconds parameter overflows in about 25 days, so instead a daily
*      check is done until the day when the deferral limit is reached, followed
*      by a non-overflowing QTimer::singleShot() call.
*/
void MessageWin::checkDeferralLimit()
{
	if (!mDeferButton  ||  !mDeferLimit.isValid())
		return;
	int n = QDate::currentDate().daysTo(mDeferLimit.date());
	if (n > 0)
		return;
	MidnightTimer::disconnect(this, SLOT(checkDeferralLimit()));
	if (n == 0)
	{
		// The deferral limit will be reached today
		n = QTime::currentTime().secsTo(mDeferLimit.time());
		if (n > 0)
		{
			QTimer::singleShot(n * 1000, this, SLOT(checkDeferralLimit()));
			return;
		}
	}
	mDeferButton->setEnabled(false);
	mDisableDeferral = true;
}

/******************************************************************************
*  Called when the Defer... button is clicked.
*  Displays the defer message dialog.
*/
void MessageWin::slotDefer()
{
	mDeferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), QDateTime::currentDateTime().addSecs(60),
	                              false, this, "deferDlg");
	if (mDefaultDeferMinutes > 0)
		mDeferDlg->setDeferMinutes(mDefaultDeferMinutes);
	mDeferDlg->setLimit(mEventID);
	if (!Preferences::modalMessages())
		lower();
	if (mDeferDlg->exec() == QDialog::Accepted)
	{
		DateTime dateTime  = mDeferDlg->getDateTime();
		int      delayMins = mDeferDlg->deferMinutes();
		const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::activeCalendar()->event(mEventID);
		if (kcalEvent)
		{
			// The event still exists in the calendar file.
			KAEvent event(*kcalEvent);
			bool repeat = event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
			event.setDeferDefaultMinutes(delayMins);
			KAlarm::updateEvent(event, 0, mDeferDlg, true, !repeat);
			if (event.deferred())
				mNoPostAction = true;
		}
		else
		{
			KAEvent event;
			kcalEvent = AlarmCalendar::displayCalendar()->event(KAEvent::uid(mEventID, KAEvent::DISPLAYING));
			if (kcalEvent)
			{
				event.reinstateFromDisplaying(KAEvent(*kcalEvent));
				event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
			}
			else
			{
				// The event doesn't exist any more !?!, so create a new one
				event.set(dateTime.dateTime(), mMessage, mBgColour, mFgColour, mFont, mAction, mLateCancel, mFlags);
				event.setAudioFile(mAudioFile, mVolume, mFadeVolume, mFadeSeconds);
				event.setArchive();
				event.setEventID(mEventID);
			}
			event.setDeferDefaultMinutes(delayMins);
			// Add the event back into the calendar file, retaining its ID
			// and not updating KOrganizer
			KAlarm::addEvent(event, 0, mDeferDlg, true, false);
			if (event.deferred())
				mNoPostAction = true;
			if (kcalEvent)
			{
				event.setUid(KAEvent::EXPIRED);
				KAlarm::deleteEvent(event, false);
			}
		}
		if (theApp()->wantRunInSystemTray())
		{
			// Alarms are to be displayed only if the system tray icon is running,
			// so start it if necessary so that the deferred alarm will be shown.
			theApp()->displayTrayIcon(true);
		}
		mNoCloseConfirm = true;   // allow window to close without confirmation prompt
		close();
	}
	else
		raise();
	delete mDeferDlg;
	mDeferDlg = 0;
}

/******************************************************************************
*  Called when the KAlarm icon button in the message window is clicked.
*  Displays the main window, with the appropriate alarm selected.
*/
void MessageWin::displayMainWindow()
{
	KAlarm::displayMainWindowSelected(mEventID);
}

/******************************************************************************
* Check whether the specified error message is already displayed for this
* alarm, and note that it will now be displayed.
* Reply = true if message is already displayed.
*/
bool MessageWin::haveErrorMessage(unsigned msg) const
{
	if (!mErrorMessages.contains(mEventID))
		mErrorMessages.insert(mEventID, 0);
	bool result = (mErrorMessages[mEventID] & msg);
	mErrorMessages[mEventID] |= msg;
	return result;
}

void MessageWin::clearErrorMessage(unsigned msg) const
{
	if (mErrorMessages.contains(mEventID))
	{
		if (mErrorMessages[mEventID] == msg)
			mErrorMessages.remove(mEventID);
		else
			mErrorMessages[mEventID] &= ~msg;
	}
}


/*=============================================================================
= Class MWMimeSourceFactory
* Gets the mime type of a text file from not only its extension (as per
* QMimeSourceFactory), but also from its contents. This allows the detection
* of plain text files without file name extensions.
=============================================================================*/
MWMimeSourceFactory::MWMimeSourceFactory(const QString& absPath, KTextBrowser* view)
	: QMimeSourceFactory(),
	  mMimeType("text/plain"),
	  mLast(0)
{
	view->setMimeSourceFactory(this);
	QString type = KMimeType::findByPath(absPath)->name();
	switch (KAlarm::fileType(type))
	{
		case KAlarm::TextPlain:
		case KAlarm::TextFormatted:
			mMimeType = type.latin1();
			// fall through to 'TextApplication'
		case KAlarm::TextApplication:
		default:
			// It's assumed to be a text file
			mTextFile = absPath;
			view->QTextBrowser::setSource(absPath);
			break;

		case KAlarm::Image:
			// It's an image file
			QString text = "<img source=\"";
			text += absPath;
			text += "\">";
			view->setText(text);
			break;
	}
	setFilePath(QFileInfo(absPath).dirPath(true));
}

MWMimeSourceFactory::~MWMimeSourceFactory()
{
	delete mLast;
}

const QMimeSource* MWMimeSourceFactory::data(const QString& abs_name) const
{
	if (abs_name == mTextFile)
	{
		QFileInfo fi(abs_name);
		if (fi.isReadable())
		{
			QFile f(abs_name);
			if (f.open(IO_ReadOnly)  &&  f.size())
			{
				QByteArray ba(f.size());
				f.readBlock(ba.data(), ba.size());
				QStoredDrag* sr = new QStoredDrag(mMimeType);
				sr->setEncodedData(ba);
				delete mLast;
				mLast = sr;
				return sr;
			}
		}
	}
	return QMimeSourceFactory::data(abs_name);
}
