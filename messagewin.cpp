/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <string.h>

#include <qfile.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qdragobject.h>
#define KDE2_QTEXTEDIT_VIEW     // for KDE2 QTextEdit compatibility
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
#include <kmimetype.h>
#include <kmessagebox.h>
#include <kwin.h>
#include <kwinmodule.h>
#include <kprocess.h>
#include <kio/netaccess.h>
#include <knotifyclient.h>
#include <kpushbutton.h>
#include <kglobalsettings.h>
#ifdef WITHOUT_ARTS
#include <kaudioplayer.h>
#else
#include <arts/kartsdispatcher.h>
#include <arts/kartsserver.h>
#include <arts/kplayobjectfactory.h>
#include <arts/kplayobject.h>
#endif
#include <kdebug.h>

#include "alarmcalendar.h"
#include "deferdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "synchtimer.h"
#include "messagewin.moc"

using namespace KCal;


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
		virtual QSize sizeHint() const  { return QSize(contentsWidth(), contentsHeight() + scrollBarHeight()); }
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


QPtrList<MessageWin> MessageWin::mWindowList;


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAEvent& event, const KAAlarm& alarm, bool reschedule_event, bool allowDefer)
	: MainWindowBase(0, "MessageWin", WFLAGS | Qt::WGroupLeader | Qt::WStyle_ContextHelp
	                                         | (Preferences::instance()->modalMessages() ? 0 : Qt::WX11BypassWM)),
	  mMessage(event.cleanText()),
	  mFont(event.font()),
	  mBgColour(event.bgColour()),
	  mFgColour(event.fgColour()),
	  mDateTime((alarm.type() & KAAlarm::REMINDER_ALARM) ? event.mainDateTime() : alarm.dateTime()),
	  mEventID(event.id()),
	  mAudioFile(event.audioFile()),
	  mVolume(event.soundVolume()),
	  mAlarmType(alarm.type()),
	  mAction(event.action()),
	  mRestoreHeight(0),
	  mAudioRepeat(event.repeatSound()),
	  mConfirmAck(event.confirmAck()),
	  mNoDefer(!allowDefer || alarm.repeatAtLogin()),
	  mInvalid(false),
	  mArtsDispatcher(0),
	  mPlayObject(0),
	  mOldVolume(-1),
	  mEvent(event),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mFlags(event.flags()),
	  mErrorWindow(false),
	  mNoPostAction(false),
	  mRecreating(false),
	  mBeep(event.beep()),
	  mRescheduleEvent(reschedule_event),
	  mShown(false),
	  mPositioning(false),
	  mDeferClosing(false),
	  mDeferDlgShowing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWin"));     // save window sizes etc.
	initView();
	mWindowList.append(this);
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
	  mErrorMsgs(errmsgs),
	  mRestoreHeight(0),
	  mConfirmAck(false),
	  mNoDefer(true),
	  mInvalid(false),
	  mArtsDispatcher(0),
	  mPlayObject(0),
	  mEvent(event),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mErrorWindow(true),
	  mNoPostAction(true),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mDeferClosing(false),
	  mDeferDlgShowing(false)
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
	  mSilenceButton(0),
	  mErrorWindow(false),
	  mNoPostAction(true),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mDeferClosing(false),
	  mDeferDlgShowing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin()\n";
	mWindowList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageWin::~MessageWin()
{
	kdDebug(5950) << "MessageWin::~MessageWin()\n";
	stopPlay();
	for (MessageWin* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w == this)
		{
			mWindowList.remove();
			break;
		}
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
				if (KIO::NetAccess::download(url, tmpFile))
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
					bool exists = KIO::NetAccess::exists(url);
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
				int h = text->sizeHint().height();
				text->setMinimumHeight(h - text->scrollBarHeight());
				text->setMaximumHeight(h);
				QWhatsThis::add(text, i18n("The alarm message"));
				int lineSpacing = text->fontMetrics().lineSpacing();
				int vspace = lineSpacing/2 - marginKDE2;
				int hspace = lineSpacing - marginKDE2 - KDialog::marginHint();
				topLayout->addSpacing(vspace);
				topLayout->addStretch();
				// Don't include any horizontal margins if message is 2/3 screen width
				if (text->sizeHint().width() >= KWinModule(0, KWinModule::INFO_DESKTOP).workArea().width()*2/3)
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
				DailyTimer::connect(this, SLOT(setRemainingTextDay()));    // update every day
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
	}

#ifndef WITHOUT_ARTS
	if (!mAudioFile.isEmpty())
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

	// KAlarm button
	KIconLoader iconLoader;
	QPixmap pixmap = iconLoader.loadIcon(QString::fromLatin1(kapp->aboutData()->appName()), KIcon::MainToolbar);
	mKAlarmButton = new QPushButton(topWidget);
	mKAlarmButton->setPixmap(pixmap);
	mKAlarmButton->setFixedSize(mKAlarmButton->sizeHint());
	connect(mKAlarmButton, SIGNAL(clicked()), SLOT(displayMainWindow()));
	grid->addWidget(mKAlarmButton, 0, gridIndex++, AlignHCenter);
	QString actKAlarm = i18n("Activate %1").arg(kapp->aboutData()->programName());
	QToolTip::add(mKAlarmButton, actKAlarm);
	QWhatsThis::add(mKAlarmButton, actKAlarm);

	// Disable all buttons initially, to prevent accidental clicking on if they happento be
	// under the mouse just as the window appears.
	mOkButton->setEnabled(false);
	if (mDeferButton)
		mDeferButton->setEnabled(false);
	mKAlarmButton->setEnabled(false);

	topLayout->activate();
	setMinimumSize(QSize(grid->sizeHint().width() + 2*KDialog::marginHint(), sizeHint().height()));

	WId winid = winId();
	unsigned long wstate = (Preferences::instance()->modalMessages() ? NET::Modal : 0) | NET::Sticky | NET::StaysOnTop;
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
	if (days == 0  &&  !mDateTime.isDateOnly())
	{
		// The alarm is due today, so start refreshing every minute
		DailyTimer::disconnect(this);
		setRemainingTextMinute();
		MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
	}
	else
	{
		if (days == 0)
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
		text = i18n("in 1 minute's time", "in %n minutes' time", mins);
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
#ifndef WITHOUT_ARTS
		if (mAudioRepeat  &&  mSilenceButton  &&  mSilenceButton->isEnabled())
		{
			// Only need to restart sound file playing if it's being repeated
			config->writeEntry(QString::fromLatin1("AudioFile"), mAudioFile);
			config->writeEntry(QString::fromLatin1("Volume"), static_cast<int>(mVolume * 100));
		}
#endif
		config->writeEntry(QString::fromLatin1("Height"), height());
		config->writeEntry(QString::fromLatin1("NoDefer"), mNoDefer);
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
	mInvalid       = config->readBoolEntry(QString::fromLatin1("Invalid"), false);
	mEventID       = config->readEntry(QString::fromLatin1("EventID"));
	mAlarmType     = KAAlarm::Type(config->readNumEntry(QString::fromLatin1("AlarmType")));
	mMessage       = config->readEntry(QString::fromLatin1("Message"));
	mAction        = KAEvent::Action(config->readNumEntry(QString::fromLatin1("Type")));
	mFont          = config->readFontEntry(QString::fromLatin1("Font"));
	mBgColour      = config->readColorEntry(QString::fromLatin1("BgColour"));
	mFgColour      = config->readColorEntry(QString::fromLatin1("FgColour"));
	mConfirmAck    = config->readBoolEntry(QString::fromLatin1("ConfirmAck"));
	QDateTime invalidDateTime;
	QDateTime dt   = config->readDateTimeEntry(QString::fromLatin1("Time"), &invalidDateTime);
	bool dateOnly  = config->readBoolEntry(QString::fromLatin1("DateOnly"));
	mDateTime.set(dt, dateOnly);
#ifndef WITHOUT_ARTS
	mAudioFile     = config->readEntry(QString::fromLatin1("AudioFile"));
	mVolume        = static_cast<float>(config->readNumEntry(QString::fromLatin1("Volume"))) / 100;
	if (!mAudioFile.isEmpty())
		mAudioRepeat = true;
#endif
	mRestoreHeight = config->readNumEntry(QString::fromLatin1("Height"));
	mNoDefer       = config->readBoolEntry(QString::fromLatin1("NoDefer"));
	if (mAlarmType != KAAlarm::INVALID_ALARM)
	{
		// Recreate the event from the calendar file (if possible)
		const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::activeCalendar()->event(mEventID);
		if (kcalEvent)
			mEvent.set(*kcalEvent);
		initView();
	}
}

/******************************************************************************
*  Returns the existing message window (if any) which is displaying the event
*  with the specified ID.
*/
MessageWin* MessageWin::findEvent(const QString& eventID)
{
	for (MessageWin* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w->mEventID == eventID  &&  !w->mErrorWindow)
			return w;
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
}

/******************************************************************************
*  Play the audio file.
*/
void MessageWin::slotPlayAudio()
{
#ifndef WITHOUT_ARTS
	// First check that it exists, to avoid possible crashes if the filename is badly specified
	KURL url(mAudioFile);
	if (!url.isValid()  ||  !KIO::NetAccess::exists(url)
	||  !KIO::NetAccess::download(url, mLocalAudioFile))
	{
		kdError(5950) << "MessageWin::playAudio(): Open failure: " << mAudioFile << endl;
		KMessageBox::error(this, i18n("Cannot open audio file:\n%1").arg(mAudioFile), kapp->aboutData()->programName());
		return;
	}
	if (!mArtsDispatcher)
	{
		mPlayTimer = new QTimer(this);
		connect(mPlayTimer, SIGNAL(timeout()), SLOT(checkAudioPlay()));
		mArtsDispatcher = new KArtsDispatcher;
		mPlayedOnce = false;
		mAudioFileLoadStart = QTime::currentTime();
		initAudio(true);
		if (!mPlayObject->object().isNull())
			checkAudioPlay();
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
		mOldVolume = sserver.outVolume().scaleFactor();    // save volume for restoration afterwards
	sserver.outVolume().scaleFactor(mVolume >= 0 ? mVolume : 1);
	mSilenceButton->setEnabled(true);
	mPlayed = false;
	connect(mPlayObject, SIGNAL(playObjectCreated()), SLOT(checkAudioPlay()));
	if (!mPlayObject->object().isNull())
		checkAudioPlay();
}
#endif

/******************************************************************************
*  Called to check whether the audio file playing has completed, and if not to
*  wait a bit longer.
*/
void MessageWin::checkAudioPlay()
{
#ifndef WITHOUT_ARTS
	if (!mPlayObject)
		return;
	if (mPlayObject->state() == Arts::posIdle)
	{
		if (mPlayedOnce  &&  !mAudioRepeat)
			return;
		kdDebug(5950) << "MessageWin::checkAudioPlay(): start\n";
		if (!mPlayedOnce)
		{
			mAudioFileLoadSecs = mAudioFileLoadStart.secsTo(QTime::currentTime());
			if (mAudioFileLoadSecs < 0)
				mAudioFileLoadSecs += 86400;
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
*  Called when the Silence button is clicked.
*  Stops playing the sound file.
*/
void MessageWin::stopPlay()
{
#ifndef WITHOUT_ARTS
	if (mArtsDispatcher)
	{
		// Restore the sound volume to what it was before the sound file
		// was played, provided that nothing else has modified it since.
		KArtsServer aserver;
		Arts::StereoVolumeControl svc = aserver.server().outVolume();
		float currentVolume = svc.scaleFactor();
		float eventVolume = mVolume;
		if (eventVolume < 0)
			eventVolume = 1;
		if (currentVolume == eventVolume)
			svc.scaleFactor(mOldVolume);
	}
	delete mPlayObject;      mPlayObject = 0;
	delete mArtsDispatcher;  mArtsDispatcher = 0;
	if (!mLocalAudioFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalAudioFile);   // removes it only if it IS a temporary file
	if (mSilenceButton)
		mSilenceButton->setEnabled(false);
#endif
}

/******************************************************************************
*  Re-output any required audio notification, and reschedule the alarm in the
*  calendar file.
*/
void MessageWin::repeat(const KAAlarm& alarm)
{
	const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::activeCalendar()->event(mEventID);
	if (kcalEvent)
	{
		mAlarmType = alarm.type();    // store new alarm type for use if it is later deferred
		if (!mDeferDlgShowing  ||  Preferences::instance()->modalMessages())
		{
			raise();
			playAudio();
		}
		KAEvent event(*kcalEvent);
		theApp()->alarmShowing(event, mAlarmType, mDateTime);
	}
}

/******************************************************************************
 * *  Display the window.
 * *  If windows are being positioned away from the mouse cursor, it is initially
 * *  positioned at the top left to slightly reduce the number of times the
 * *  windows need to be moved in showEvent().
 * */
void MessageWin::show()
{
	if (Preferences::instance()->messageButtonDelay() == 0)
		move(0, 0);
	MainWindowBase::show();
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
			QSize s = sizeHint();     // fit the window round the message
			if (mAction == KAEvent::FILE  &&  !mErrorMsgs.count())
				KAlarm::readConfigWindowSize("FileMessage", s);
			resize(s);

			if (Preferences::instance()->messageButtonDelay() == 0)
			{
				/* Try to ensure that the window can't accidentally be acknowledged
				 * by the user clicking the mouse just as it appears.
				 * To achieve this, move the window so that the OK button is as far away
				 * from the cursor as possible.
				 * N.B. This can't be done in show(), since the geometry of the window
				 *      is not known until it is displayed. Unfortunately by moving the
				 *      window in showEvent(), a flicker is unavoidable.
				 *      See the Qt documentation on window geometry for more details.
				 */
				QRect desk = KGlobalSettings::desktopGeometry(this);
				QPoint cursor = QCursor::pos();
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
				if (x != frame.left()  ||  y != frame.top())
				{
					mPositioning = true;
					move(x, y);
				}
			}
			if (!mPositioning)
				displayComplete();    // play audio, etc.
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
*  Called when the window has been displayed properly (in its correct position),
*  to play sounds and reschedule the event.
*/
void MessageWin::displayComplete()
{
	playAudio();
	if (mRescheduleEvent)
		theApp()->alarmShowing(mEvent, mAlarmType, mDateTime);
	enableButtons();
}

/******************************************************************************
 * *  Enable the window's buttons.
 * */
void MessageWin::enableButtons()
{
	mOkButton->setEnabled(true);
	mKAlarmButton->setEnabled(true);
	if (mDeferButton)
		mDeferButton->setEnabled(true);
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*/
void MessageWin::resizeEvent(QResizeEvent* re)
{
	if (mRestoreHeight)
	{
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
	if (mConfirmAck  &&  !mDeferClosing  &&  !theApp()->sessionClosingDown())
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
	MainWindowBase::closeEvent(ce);
}

/******************************************************************************
*  Called when the Defer... button is clicked.
*  Displays the defer message dialog.
*/
void MessageWin::slotDefer()
{
	DeferAlarmDlg deferDlg(i18n("Defer Alarm"), QDateTime::currentDateTime().addSecs(60),
	                       false, this, "deferDlg");
	deferDlg.setLimit(mEventID);
	mDeferDlgShowing = true;
	if (!Preferences::instance()->modalMessages())
		lower();
	if (deferDlg.exec() == QDialog::Accepted)
	{
		DateTime dateTime = deferDlg.getDateTime();
		const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::activeCalendar()->event(mEventID);
		if (kcalEvent)
		{
			// The event still exists in the calendar file.
			KAEvent event(*kcalEvent);
			event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
			KAlarm::updateEvent(event, 0);
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
				event.set(dateTime.dateTime(), mMessage, mBgColour, mFgColour, mFont, mAction, mFlags);
				event.setAudioFile(mAudioFile, mVolume);
				event.setArchive();
				event.setEventID(mEventID);
			}
			// Add the event back into the calendar file, retaining its ID
			KAlarm::addEvent(event, 0, true);
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
		mDeferClosing = true;   // allow window to close without confirmation prompt
		close();
	}
	else
		raise();
	mDeferDlgShowing = false;
}

/******************************************************************************
*  Called when the KAlarm icon button in the message window is clicked.
*  Displays the main window, with the appropriate alarm selected.
*/
void MessageWin::displayMainWindow()
{
	KAlarm::displayMainWindowSelected(mEventID);
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
