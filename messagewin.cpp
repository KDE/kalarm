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

#include <QScrollBar>
#include <QtDBus>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QPalette>
#include <QTimer>
#include <QPixmap>
#include <QByteArray>
#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QDesktopWidget>

#include <kstandarddirs.h>
#include <kaction.h>
#include <KStandardGuiItem>
#include <kaboutdata.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <ktextbrowser.h>
#include <ksystemtimezone.h>
#include <kglobalsettings.h>
#include <kmimetype.h>
#include <kmessagebox.h>
#include <kwindowsystem.h>
#include <k3process.h>
#include <kio/netaccess.h>
#include <knotification.h>
#include <kpushbutton.h>
#if 0
#include <phonon/simpleplayer.h>
#else
#include <phonon/mediaobject.h>
#include <phonon/audiopath.h>
#include <phonon/audiooutput.h>
#include <phonon/volumefadereffect.h>
#endif
#include <kdebug.h>
#include <ktoolinvocation.h>

#include "alarmcalendar.h"
#include "daemon.h"
#include "deferdlg.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kmailinterface.h"
#include "mainwindow.h"
#include "preferences.h"
#include "synchtimer.h"
#include "messagewin.moc"

using namespace KCal;

static const char* KTTSD_DBUS_SERVICE  = "org.kde.kttsd";
static const char* KTTDS_DBUS_PATH      = "/KSpeech";
static const char* KTTSD_DBUS_INTERFACE = "org.kde.KSpeech";

static const char* KMAIL_DBUS_SERVICE   = "org.kde.kmail";
static const char* KMAIL_DBUS_PATH      = "/KMail";

// The delay for enabling message window buttons if a zero delay is
// configured, i.e. the windows are placed far from the cursor.
static const int proximityButtonDelay = 1000;    // (milliseconds)
static const int proximityMultiple = 10;         // multiple of button height distance from cursor for proximity

// A text label widget which can be scrolled and copied with the mouse
class MessageText : public QTextEdit
{
	public:
		MessageText(QWidget* parent = 0)
			: QTextEdit(parent)
		{
			setReadOnly(true);
			setFrameStyle(NoFrame);
			setLineWrapMode(NoWrap);
		}
		int scrollBarHeight() const     { return horizontalScrollBar()->height(); }
		int scrollBarWidth() const      { return verticalScrollBar()->width(); }
//TODO: Restore the following line
//		virtual QSize sizeHint() const  { return QSize(contentsWidth() + scrollBarWidth(), contentsHeight() + scrollBarHeight()); }
};


// Basic flags for the window
static const Qt::WFlags          WFLAGS       = Qt::WindowStaysOnTopHint;
static const Qt::WFlags          WFLAGS2      = Qt::WindowContextHelpButtonHint;
static const Qt::WidgetAttribute WidgetFlags  = Qt::WA_DeleteOnClose;
#ifdef __GNUC__
#warning WA_GroupLeader deprecated and replaced with MainWindowBase::setWindowModality() - check
#endif
static const int WidgetFlags2 = 0;//Qt::WA_GroupLeader;
//static const Qt::WidgetAttribute WidgetFlags2 = 0;//Qt::WA_GroupLeader;


QList<MessageWin*> MessageWin::mWindowList;


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAEvent& event, const KAAlarm& alarm, int flags)
	: MainWindowBase(0, static_cast<Qt::WFlags>(WFLAGS | WFLAGS2 | (Preferences::modalMessages() ? 0 : Qt::X11BypassWindowManagerHint))),
	  mMessage(event.cleanText()),
	  mFont(event.font()),
	  mBgColour(event.bgColour()),
	  mFgColour(event.fgColour()),
	  mDateTime((alarm.type() & KAAlarm::REMINDER_ALARM) ? event.mainDateTime(true) : alarm.dateTime(true)),
	  mEventID(event.id()),
	  mAudioFile(event.audioFile()),
	  mVolume(event.soundVolume()),
	  mFadeVolume(event.fadeVolume()),
	  mFadeSeconds(qMin(event.fadeSeconds(), 86400)),
	  mDefaultDeferMinutes(event.deferDefaultMinutes()),
	  mAlarmType(alarm.type()),
	  mAction(event.action()),
	  mKMailSerialNumber(event.kmailSerialNumber()),
	  mRestoreHeight(0),
	  mAudioRepeat(event.repeatSound()),
	  mConfirmAck(event.confirmAck()),
	  mNoDefer(true),
	  mInvalid(false),
	  mAudioObject(0),
	  mEvent(event),
	  mResource(AlarmResources::instance()->resourceForIncidence(mEventID)),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mFlags(event.flags()),
	  mLateCancel(event.lateCancel()),
	  mErrorWindow(false),
	  mNoPostAction(alarm.type() & KAAlarm::REMINDER_ALARM),
	  mRecreating(false),
	  mBeep(event.beep()),
	  mSpeak(event.speak()),
	  mRescheduleEvent(!(flags & NO_RESCHEDULE)),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags | WidgetFlags2));
	setObjectName("MessageWin");    // used by LikeBack
	if (!(flags & NO_INIT_VIEW))
	{
		bool readonly = AlarmCalendar::resources()->eventReadOnly(mEventID);
		mShowEdit = !mEventID.isEmpty()  &&  !readonly;
		mNoDefer  = readonly || (flags & NO_DEFER) || alarm.repeatAtLogin();
		initView();
	}
	// Set to save settings automatically, but don't save window size.
	// File alarm window size is saved elsewhere.
	setAutoSaveSettings(QLatin1String("MessageWin"), false);
	mWindowList.append(this);
	if (event.autoClose())
		mCloseTime = alarm.dateTime().effectiveDateTime().addSecs(event.lateCancel() * 60);
}

/******************************************************************************
*  Construct the message window for a specified error message.
*/
MessageWin::MessageWin(const KAEvent& event, const DateTime& alarmDateTime, const QStringList& errmsgs)
	: MainWindowBase(0, WFLAGS | WFLAGS2),
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
	  mAudioObject(0),
	  mEvent(event),
	  mResource(0),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mErrorWindow(true),
	  mNoPostAction(true),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kDebug(5950) << "MessageWin::MessageWin(errmsg)" << endl;
	setAttribute(static_cast<Qt::WidgetAttribute>(WidgetFlags | WidgetFlags2));
	setObjectName("ErrorWin");    // used by LikeBack
	initView();
	mWindowList.append(this);
}

/******************************************************************************
*  Construct the message window for restoration by session management.
*  The window is initialised by readProperties().
*/
MessageWin::MessageWin()
	: MainWindowBase(0, WFLAGS),
	  mAudioObject(0),
	  mEditButton(0),
	  mDeferButton(0),
	  mSilenceButton(0),
	  mDeferDlg(0),
	  mErrorWindow(false),
	  mRecreating(false),
	  mRescheduleEvent(false),
	  mShown(false),
	  mPositioning(false),
	  mNoCloseConfirm(false),
	  mDisableDeferral(false)
{
	kDebug(5950) << "MessageWin::MessageWin(restore)\n";
	setAttribute(WidgetFlags);
	setObjectName("RestoredMsgWin");    // used by LikeBack
	mWindowList.append(this);
}

/******************************************************************************
* Destructor. Perform any post-alarm actions before tidying up.
*/
MessageWin::~MessageWin()
{
	kDebug(5950) << "MessageWin::~MessageWin(" << mEventID << ")" << endl;
	stopPlay();
	mWindowList.removeAll(this);
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
	QWidget* topWidget = new QWidget(this);
	setCentralWidget(topWidget);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
	topLayout->setMargin(KDialog::marginHint());
	topLayout->setSpacing(KDialog::spacingHint());

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
			layout = new QVBoxLayout(frame);
			topLayout->setMargin(leading + frame->frameWidth());
			topLayout->setSpacing(leading);
		}

		// Alarm date/time.
		// Display time zone if not local time zone.
		QLabel* label = new QLabel(frame ? frame : topWidget);
		label->setText(KGlobal::locale()->formatDateTime(KDateTime(mDateTime), KLocale::ShortDate, KLocale::DateTimeFormatOptions(mDateTime.isLocalZone() ? 0 : KLocale::TimeZone)));
		if (!frame)
			label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignHCenter);
		label->setWhatsThis(i18n("The scheduled date/time for the message (as opposed to the actual time of display)."));

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
				label->setWhatsThis(i18n("The file whose contents are displayed below"));
				topLayout->addWidget(label, 0, Qt::AlignHCenter);

				// Display contents of file
				bool opened = false;
				bool dir = false;
				QString tmpFile;
				KUrl url(mMessage);
				if (KIO::NetAccess::download(url, tmpFile, MainWindow::mainMainWindow()))
				{
					QFile qfile(tmpFile);
					QFileInfo info(qfile);
					if (!(dir = info.isDir()))
					{
						opened = true;
						KTextBrowser* view = new KTextBrowser(topWidget, "fileContents");
						bool imageType = (KAlarm::fileType(KMimeType::findByPath(tmpFile)->name()) == KAlarm::Image);
#warning Check that HTML links and link paths work
						if (imageType)
							view->setHtml("<img source=\"" + tmpFile + "\">");
						else
							view->QTextBrowser::setSource(tmpFile);   // if not an image, assume a text file
						view->setMinimumSize(view->sizeHint());
						topLayout->addWidget(view);

						// Set the default size to 20 lines square.
						// Note that after the first file has been displayed, this size
						// is overridden by the user-set default stored in the config file.
						// So there is no need to calculate an accurate size.
						int h = 20*view->fontMetrics().lineSpacing() + 2*view->frameWidth();
						view->resize(QSize(h, h).expandedTo(view->sizeHint()));
						view->setWhatsThis(i18n("The contents of the file to be displayed"));
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
				MessageText* text = new MessageText(topWidget);
				text->setAutoFillBackground(true);
				QPalette pal = text->palette();
				pal.setColor(text->backgroundRole(), mBgColour);
				text->setPalette(pal);
				pal = text->viewport()->palette();
				pal.setColor(text->viewport()->backgroundRole(), mBgColour);
				text->viewport()->setPalette(pal);
				text->setTextColor(mFgColour);
				text->setCurrentFont(mFont);
				text->insertPlainText(mMessage);
				int lineSpacing = text->fontMetrics().lineSpacing();
				QSize s = text->sizeHint();
				int h = s.height();
				text->setMaximumHeight(h + text->scrollBarHeight());
				text->setMinimumHeight(qMin(h, lineSpacing*4));
				text->setMaximumWidth(s.width() + text->scrollBarWidth());
				text->setWhatsThis(i18n("The alarm message"));
				int vspace = lineSpacing/2;
				int hspace = lineSpacing - KDialog::marginHint();
				topLayout->addSpacing(vspace);
				topLayout->addStretch();
				// Don't include any horizontal margins if message is 2/3 screen width
#ifdef Q_WS_X11
				if (text->sizeHint().width() >= KWindowSystem::workArea().width()*2/3)
					topLayout->addWidget(text, 1, Qt::AlignHCenter);
				else
#endif
				{
					QHBoxLayout* layout = new QHBoxLayout();
					layout->addSpacing(hspace);
					layout->addWidget(text, 1, Qt::AlignHCenter);
					layout->addSpacing(hspace);
					topLayout->addLayout(layout);
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
				frame->setWhatsThis(i18n("The email to send"));
				topLayout->addWidget(frame, 0, Qt::AlignHCenter);
				QGridLayout* grid = new QGridLayout(frame);
				grid->setMargin(KDialog::marginHint());
				grid->setSpacing(KDialog::spacingHint());

				QLabel* label = new QLabel(i18nc("Email addressee", "To:"), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 0, 0, Qt::AlignLeft);
				label = new QLabel(mEvent.emailAddresses("\n"), frame);
				label->setFixedSize(label->sizeHint());
				grid->addWidget(label, 0, 1, Qt::AlignLeft);

				label = new QLabel(i18nc("Email subject", "Subject:"), frame);
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
	{
		topWidget->setAutoFillBackground(true);
		QPalette palette;
		palette.setColor(topWidget->backgroundRole(), mBgColour);
		topWidget->setPalette(palette);
	}
	else
	{
		setCaption(i18n("Error"));
		QHBoxLayout* layout = new QHBoxLayout();
		layout->setMargin(2*KDialog::marginHint());
		layout->addStretch();
		topLayout->addLayout(layout);
		QLabel* label = new QLabel(topWidget);
		label->setPixmap(DesktopIcon("error"));
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignRight);
		QVBoxLayout* vlayout = new QVBoxLayout();
		layout->addLayout(vlayout);
		for (QStringList::Iterator it = mErrorMsgs.begin();  it != mErrorMsgs.end();  ++it)
		{
			label = new QLabel(*it, topWidget);
			label->setFixedSize(label->sizeHint());
			vlayout->addWidget(label, 0, Qt::AlignLeft);
		}
		layout->addStretch();
	}

	QGridLayout* grid = new QGridLayout();
	grid->setColumnStretch(0, 1);     // keep the buttons right-adjusted in the window
	topLayout->addLayout(grid);
	int gridIndex = 1;

	// Close button
	mOkButton = new KPushButton(KStandardGuiItem::close(), topWidget);
	// Prevent accidental acknowledgement of the message if the user is typing when the window appears
	mOkButton->clearFocus();
	mOkButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
	mOkButton->setFixedSize(mOkButton->sizeHint());
	connect(mOkButton, SIGNAL(clicked()), SLOT(close()));
	grid->addWidget(mOkButton, 0, gridIndex++, Qt::AlignHCenter);
	mOkButton->setWhatsThis(i18n("Acknowledge the alarm"));

	if (mShowEdit)
	{
		// Edit button
		mEditButton = new QPushButton(i18n("&Edit..."), topWidget);
		mEditButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
		mEditButton->setFixedSize(mEditButton->sizeHint());
		connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
		grid->addWidget(mEditButton, 0, gridIndex++, Qt::AlignHCenter);
		mEditButton->setWhatsThis(i18n("Edit the alarm."));
	}

	if (!mNoDefer)
	{
		// Defer button
		mDeferButton = new QPushButton(i18n("&Defer..."), topWidget);
		mDeferButton->setFocusPolicy(Qt::ClickFocus);    // don't allow keyboard selection
		mDeferButton->setFixedSize(mDeferButton->sizeHint());
		connect(mDeferButton, SIGNAL(clicked()), SLOT(slotDefer()));
		grid->addWidget(mDeferButton, 0, gridIndex++, Qt::AlignHCenter);
		mDeferButton->setWhatsThis(i18n("Defer the alarm until later.\n"
		                                "You will be prompted to specify when the alarm should be redisplayed."));

		setDeferralLimit(mEvent);    // ensure that button is disabled when alarm can't be deferred any more
	}

	if (!mAudioFile.isEmpty()  &&  (mVolume || mFadeVolume > 0))
	{
		// Silence button to stop sound repetition
		QPixmap pixmap = MainBarIcon("media-playback-stop");
		mSilenceButton = new QPushButton(topWidget);
		mSilenceButton->setIcon(pixmap);
		mSilenceButton->setFixedSize(mSilenceButton->sizeHint());
		connect(mSilenceButton, SIGNAL(clicked()), SLOT(stopPlay()));
		grid->addWidget(mSilenceButton, 0, gridIndex++, Qt::AlignHCenter);
		mSilenceButton->setToolTip(i18n("Stop sound"));
		mSilenceButton->setWhatsThis(i18n("Stop playing the sound"));
		// To avoid getting in a mess, disable the button until sound playing has been set up
		mSilenceButton->setEnabled(false);
	}

	KIconLoader iconLoader;
	if (mKMailSerialNumber)
	{
		// KMail button
		QPixmap pixmap = iconLoader.loadIcon(QLatin1String("kmail"), K3Icon::MainToolbar);
		mKMailButton = new QPushButton(topWidget);
		mKMailButton->setIcon(pixmap);
		mKMailButton->setFixedSize(mKMailButton->sizeHint());
		connect(mKMailButton, SIGNAL(clicked()), SLOT(slotShowKMailMessage()));
		grid->addWidget(mKMailButton, 0, gridIndex++, Qt::AlignHCenter);
		mKMailButton->setToolTip(i18nc("Locate this email in KMail", "Locate in KMail"));
		mKMailButton->setWhatsThis(i18n("Locate and highlight this email in KMail"));
	}
	else
		mKMailButton = 0;

	// KAlarm button
	QPixmap pixmap = iconLoader.loadIcon(QLatin1String(KGlobal::mainComponent().aboutData()->appName()), K3Icon::MainToolbar);
	mKAlarmButton = new QPushButton(topWidget);
	mKAlarmButton->setIcon(pixmap);
	mKAlarmButton->setFixedSize(mKAlarmButton->sizeHint());
	connect(mKAlarmButton, SIGNAL(clicked()), SLOT(displayMainWindow()));
	grid->addWidget(mKAlarmButton, 0, gridIndex++, Qt::AlignHCenter);
	QString actKAlarm = i18n("Activate KAlarm");
	mKAlarmButton->setToolTip(actKAlarm);
	mKAlarmButton->setWhatsThis(actKAlarm);

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
#ifdef Q_OS_UNIX
	WId winid = winId();
	unsigned long wstate = (Preferences::modalMessages() ? NET::Modal : 0) | NET::Sticky | NET::StaysOnTop;
	KWindowSystem::setState(winid, wstate);
	KWindowSystem::setOnAllDesktops(winid, true);
#endif
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
		MidnightTimer::disconnect(this, SLOT(setRemainingTextDay()));
		setRemainingTextMinute();
		MinuteTimer::connect(this, SLOT(setRemainingTextMinute()));   // update every minute
	}
	else
	{
		if (days == 0)
			text = i18n("Today");
		else if (days % 7)
			text = i18np("Tomorrow", "in %1 days' time", days);
		else
			text = i18np("in 1 week's time", "in %1 weeks' time", days/7);
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
	int mins = (KDateTime::currentUtcDateTime().secsTo(mDateTime.effectiveKDateTime()) + 59) / 60;
	if (mins < 60)
		text = i18np("in 1 minute's time", "in %1 minutes' time", mins);
	else if (mins % 60 == 0)
		text = i18np("in 1 hour's time", "in %1 hours' time", mins/60);
	else if (mins % 60 == 1)
		text = i18np("in 1 hour 1 minute's time", "in %1 hours 1 minute's time", mins/60);
	else
		text = i18np("in 1 hour %2 minutes' time", "in %1 hours %2 minutes' time", mins/60, mins%60);
	mRemainingText->setText(text);
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWin::saveProperties(KConfigGroup& config)
{
	if (mShown  &&  !mErrorWindow)
	{
		config.writeEntry("EventID", mEventID);
		config.writeEntry("AlarmType", static_cast<int>(mAlarmType));
		config.writeEntry("Message", mMessage);
		config.writeEntry("Type", static_cast<int>(mAction));
		config.writeEntry("Font", mFont);
		config.writeEntry("BgColour", mBgColour);
		config.writeEntry("FgColour", mFgColour);
		config.writeEntry("ConfirmAck", mConfirmAck);
		if (mDateTime.isValid())
		{
//TODO: Write KDateTime when it becomes possible
			config.writeEntry("Time", mDateTime.effectiveDateTime());
			config.writeEntry("DateOnly", mDateTime.isDateOnly());
			QString zone;
			if (mDateTime.isUtc())
				zone = QLatin1String("UTC");
			else
			{
				const KTimeZone* tz = mDateTime.timeZone();
				if (tz)
					zone = tz->name();
			}
			config.writeEntry("TimeZone", zone);
		}
		if (mCloseTime.isValid())
			config.writeEntry("Expiry", mCloseTime);
		if (mAudioRepeat  &&  mSilenceButton  &&  mSilenceButton->isEnabled())
		{
			// Only need to restart sound file playing if it's being repeated
			config.writePathEntry(QLatin1String("AudioFile"), mAudioFile);
			config.writeEntry("Volume", static_cast<int>(mVolume * 100));
		}
		config.writeEntry("Speak", mSpeak);
		config.writeEntry("Height", height());
		config.writeEntry("DeferMins", mDefaultDeferMinutes);
		config.writeEntry("NoDefer", mNoDefer);
		config.writeEntry("NoPostAction", mNoPostAction);
		config.writeEntry("KMailSerial", static_cast<qulonglong>(mKMailSerialNumber));
	}
	else
		config.writeEntry("Invalid", true);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(const KConfigGroup& config)
{
	mInvalid             = config.readEntry("Invalid", false);
	mEventID             = config.readEntry("EventID");
	mAlarmType           = static_cast<KAAlarm::Type>(config.readEntry("AlarmType", 0));
	mMessage             = config.readEntry("Message");
	mAction              = static_cast<KAEvent::Action>(config.readEntry("Type", 0));
	mFont                = config.readEntry("Font", QFont());
	mBgColour            = config.readEntry("BgColour", QColor(Qt::white));
	mFgColour            = config.readEntry("FgColour", QColor(Qt::black));
	mConfirmAck          = config.readEntry("ConfirmAck", false);
	QDateTime invalidDateTime;
	QDateTime dt         = config.readEntry("Time", invalidDateTime);
	QString zone         = config.readEntry("TimeZone");
	if (zone.isEmpty())
		mDateTime = KDateTime(dt, KDateTime::ClockTime);
	else if (zone == QString::fromLatin1("UTC"))
	{
		dt.setTimeSpec(Qt::UTC);
		mDateTime = KDateTime(dt, KDateTime::UTC);
	}
	else
	{
		const KTimeZone* tz = KSystemTimeZones::zone(zone);
		mDateTime = KDateTime(dt, (tz ? tz : KSystemTimeZones::local()));
	}
	bool dateOnly        = config.readEntry("DateOnly", false);
	if (dateOnly)
		mDateTime.setDateOnly(true);
	mCloseTime           = config.readEntry("Expiry", invalidDateTime);
	mAudioFile           = config.readPathEntry(QLatin1String("AudioFile"));
	mVolume              = static_cast<float>(config.readEntry("Volume", 0)) / 100;
	mFadeVolume          = -1;
	mFadeSeconds         = 0;
	if (!mAudioFile.isEmpty())
		mAudioRepeat = true;
	mSpeak               = config.readEntry("Speak", false);
	mRestoreHeight       = config.readEntry("Height", 0);
	mDefaultDeferMinutes = config.readEntry("DeferMins", 0);
	mNoDefer             = config.readEntry("NoDefer", false);
	mNoPostAction        = config.readEntry("NoPostAction", true);
	mKMailSerialNumber   = static_cast<unsigned long>(config.readEntry("KMailSerial", QVariant(QVariant::ULongLong)).toULongLong());
	mShowEdit            = false;
	mResource            = 0;
	kDebug(5950) << "MessageWin::readProperties(" << mEventID << ")" << endl;
	if (mAlarmType != KAAlarm::INVALID_ALARM)
	{
		// Recreate the event from the calendar file (if possible)
		if (!mEventID.isEmpty())
		{
			const Event* kcalEvent = AlarmCalendar::resources()->event(mEventID);
			if (kcalEvent)
			{
				mEvent.set(kcalEvent);
				mResource = AlarmResources::instance()->resource(kcalEvent);
				mShowEdit = true;
			}
			else
			{
				// It's not in the active calendar, so try the displaying or archive calendars
				retrieveEvent(mEvent, mResource, mShowEdit, mNoDefer);
				mNoDefer = !mNoDefer;
			}
		}
		initView();
	}
}

/******************************************************************************
*  Redisplay alarms which were being shown when the program last exited.
*  Normally, these alarms will have been displayed by session restoration, but
*  if the program crashed or was killed, we can redisplay them here so that
*  they won't be lost.
*/
void MessageWin::redisplayAlarms()
{
	AlarmCalendar* cal = AlarmCalendar::displayCalendar();
	if (cal->isOpen())
	{
		KAEvent event;
		AlarmResource* resource;
		KCal::Event::List events = cal->events();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			bool showDefer, showEdit;
			reinstateFromDisplaying(events[i], event, resource, showEdit, showDefer);
			if (!findEvent(event.id()))
			{
				// This event should be displayed, but currently isn't being
				kDebug(5950) << "MessageWin::redisplayAlarms(): " << event.id() << endl;
				KAAlarm alarm = event.convertDisplayingAlarm();
				bool login = alarm.repeatAtLogin();
				int flags = NO_RESCHEDULE | (login ? NO_DEFER : 0) | NO_INIT_VIEW;
				MessageWin* win = new MessageWin(event, alarm, flags);
				win->mResource = resource;
				bool rw = resource  &&  resource->writable();
				win->mShowEdit = rw ? showEdit : false;
				win->mNoDefer  = (rw && !login) ? !showDefer : true;
				win->initView();
				win->show();
			}
		}
	}
}

/******************************************************************************
*  Retrieves the event with the current ID from the displaying calendar file,
*  or if not found there, from the archive calendar.
*/
bool MessageWin::retrieveEvent(KAEvent& event, AlarmResource*& resource, bool& showEdit, bool& showDefer)
{
	const Event* kcalEvent = AlarmCalendar::displayCalendar()->event(KCalEvent::uid(mEventID, KCalEvent::DISPLAYING));
	if (!reinstateFromDisplaying(kcalEvent, event, resource, showEdit, showDefer))
	{
		// The event isn't in the displaying calendar.
		// Try to retrieve it from the archive calendar.
		kcalEvent = AlarmCalendar::resources()->event(KCalEvent::uid(mEventID, KCalEvent::ARCHIVED));
		if (!kcalEvent)
			return false;
		event.set(kcalEvent);
		event.setArchive();     // ensure that it gets re-archived if it's saved
		event.setCategory(KCalEvent::ACTIVE);
		if (mEventID != event.id())
			kError(5950) << "MessageWin::retrieveEvent(): wrong event ID" << endl;
		event.setEventID(mEventID);
		resource  = 0;
		showEdit  = true;
		showDefer = true;
	}
	return true;
}

/******************************************************************************
*  Retrieves the displayed event from the calendar file, or if not found there,
*  from the displaying calendar.
*/
bool MessageWin::reinstateFromDisplaying(const Event* kcalEvent, KAEvent& event, AlarmResource*& resource, bool& showEdit, bool& showDefer)
{
	if (!kcalEvent)
		return false;
	QString resourceID;
	event.reinstateFromDisplaying(kcalEvent, resourceID, showEdit, showDefer);
	resource = AlarmResources::instance()->resourceWithId(resourceID);
	if (resource  &&  !resource->isOpen())
		resource = 0;
	event.clearResourceID();
	return true;
}

/******************************************************************************
* Called when an alarm is currently being displayed, to store a copy of the
* alarm in the displaying calendar, and to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
void MessageWin::alarmShowing(KAEvent& event, const KCal::Event* kcalEvent)
{
	kDebug(5950) << "MessageWin::alarmShowing(" << event.id() << ", " << KAAlarm::debugType(mAlarmType) << ")\n";
	if (!kcalEvent)
		kcalEvent = AlarmCalendar::resources()->event(event.id());
	if (!kcalEvent)
		kError(5950) << "MessageWin::alarmShowing(): event ID not found: " << event.id() << endl;
	else
	{
		KAAlarm alarm = event.alarm(mAlarmType);
		if (!alarm.valid())
			kError(5950) << "MessageWin::alarmShowing(): alarm type not found: " << event.id() << ":" << mAlarmType << endl;
		else
		{
			// Copy the alarm to the displaying calendar in case of a crash, etc.
			AlarmResource* resource = AlarmResources::instance()->resource(kcalEvent);
			KAEvent dispEvent;
			dispEvent.setDisplaying(event, mAlarmType, (resource ? resource->identifier() : QString()),
			                        mDateTime.effectiveKDateTime(), mShowEdit, !mNoDefer);
			AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
			if (cal)
			{
				cal->deleteEvent(dispEvent.id());   // in case it already exists
				cal->addEvent(dispEvent);
				cal->save();
			}

			theApp()->rescheduleAlarm(event, alarm);
			return;
		}
	}
	Daemon::eventHandled(event.id());
}

/******************************************************************************
*  Returns the existing message window (if any) which is displaying the event
*  with the specified ID.
*/
MessageWin* MessageWin::findEvent(const QString& eventID)
{
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
	{
		MessageWin* w = mWindowList[i];
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
		KNotification::beep();     // beep through the sound card & speakers
		QApplication::beep();      // beep through the internal speaker
	}
	if (!mAudioFile.isEmpty())
	{
		if (!mVolume  &&  mFadeVolume <= 0)
			return;    // ensure zero volume doesn't play anything
		QString play = mAudioFile;
		QString file = QLatin1String("file:");
		if (mAudioFile.startsWith(file))
			play = mAudioFile.mid(file.length());
#if 0
		Phonon::SimplePlayer* player = new Phonon::SimplePlayer(this);
		player->play(KUrl(QFile::encodeName(play)));
#else
		// An audio file is specified. Because loading it may take some time,
		// call it on a timer to allow the window to display first.
		Phonon::AudioOutput* output = new Phonon::AudioOutput(Phonon::NotificationCategory, this);
		output->setVolume(mVolume);
		Phonon::AudioPath* path = new Phonon::AudioPath(this);
		path->addOutput(output);
		mAudioObject = new Phonon::MediaObject(this);
		mAudioObject->setCurrentSource(play);
		mAudioObject->addAudioPath(path);
		if (mFadeVolume >= 0  &&  mFadeSeconds > 0)
		{
			Phonon::VolumeFaderEffect* fader = new Phonon::VolumeFaderEffect(this);
			fader->setVolume(mFadeVolume);
			fader->fadeIn(mFadeSeconds);
			path->insertEffect(fader);
		}
		connect(mAudioObject, SIGNAL(finished()), SLOT(checkAudioPlay()));
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
	QDBusConnection client = QDBusConnection::sessionBus();
	if (!client.interface()->isServiceRegistered(KTTSD_DBUS_SERVICE))
	{
		// kttsd is not running, so start it
		QString error;
		if (KToolInvocation::startServiceByDesktopName(QLatin1String("kttsd"), QStringList(), &error))
		{
			kDebug(5950) << "MessageWin::slotSpeak(): failed to start kttsd: " << error << endl;
			KMessageBox::detailedError(0, i18n("Unable to speak message"), error);
			return;
		}
	}
	// FIXME: this would be a lot cleaner just using kttsd's dbus stub.
	QDBusInterface kttsdDBus(KTTSD_DBUS_SERVICE, KTTDS_DBUS_PATH, KTTSD_DBUS_INTERFACE );
	QDBusMessage reply = kttsdDBus.call(QLatin1String("sayMessage"), mMessage, QString());
	if (reply.type() == QDBusMessage::ErrorMessage)
	{
		kDebug(5950) << "MessageWin::slotSpeak(): sayMessage() D-Bus error" << endl;
		KMessageBox::detailedError(0, i18n("Unable to speak message"), i18n("DBus call sayMessage failed"));
	}
}

/******************************************************************************
*  Play the audio file.
*  Called asynchronously to avoid delaying the display of the message.
*/
void MessageWin::slotPlayAudio()
{
	// First check that it exists, to avoid possible crashes if the filename is badly specified
	MainWindow* mmw = MainWindow::mainMainWindow();
#ifdef __GNUC__
#warning Use mmw for error messages
#endif
	if (0)
	{
		kError(5950) << "MessageWin::playAudio(): Open failure: " << mAudioFile << endl;
		KMessageBox::error(this, i18n("Cannot open audio file:\n%1", mAudioFile));
		return;
	}
	mPlayedOnce = false;
	mSilenceButton->setEnabled(true);
	checkAudioPlay();
}

/******************************************************************************
*  Called when the audio file has loaded and is ready to play, or on a timer
*  when play is expected to have completed.
*  If it is ready to play, start playing it (for the first time or repeated).
*  If play has not yet completed, wait a bit longer.
*/
void MessageWin::checkAudioPlay()
{
	if (!mAudioObject)
		return;
	// The file has loaded and is ready to play, or play has completed
	if (mPlayedOnce  &&  !mAudioRepeat)
	{
		// Play has completed
		stopPlay();
		return;
	}
	mPlayedOnce = true;

	// Start playing the file, either for the first time or again
	kDebug(5950) << "MessageWin::checkAudioPlay(): start\n";
	mAudioObject->play();
}

/******************************************************************************
*  Called when play completes, the Silence button is clicked, or the window is
*  closed, to terminate audio access.
*/
void MessageWin::stopPlay()
{
	if (mAudioObject)
	{
		mAudioObject->stop();
		delete mAudioObject;
	}
	if (mSilenceButton)
		mSilenceButton->setEnabled(false);
}

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
	const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::resources()->event(mEventID);
	if (kcalEvent)
	{
		mAlarmType = alarm.type();    // store new alarm type for use if it is later deferred
		if (!mDeferDlg  ||  Preferences::modalMessages())
		{
			raise();
			playAudio();
		}
		KAEvent event(kcalEvent);
		mDeferButton->setEnabled(true);
		setDeferralLimit(event);    // ensure that button is disabled when alarm can't be deferred any more
		alarmShowing(event);
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
#ifdef Q_WS_X11
	QSize desktop  = KWindowSystem::workArea().size();
#else
	QSize desktop  = qApp->desktop()->availableGeometry().size();
#endif
	QSize frame = frameGeometry().size();
	QSize contents = geometry().size();
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
	if (mShown)
		return;
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
#ifdef Q_WS_X11
			QRect desk = KWindowSystem::workArea();
#else
			QRect desk = qApp->desktop()->availableGeometry();
#endif
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

			// Find the enclosing rectangle for the new button positions
			// and check if the cursor is too near
			QRect buttons = mOkButton->geometry().unite(mKAlarmButton->geometry());
			buttons.translate(rect.left() + x - frame.left(), rect.top() + y - frame.top());
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
		alarmShowing(mEvent);

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
			                                    i18n("Acknowledge Alarm"), KGuiItem(i18n("&Acknowledge")), KStandardGuiItem::cancel())
			    != KMessageBox::Yes)
			{
				ce->ignore();
				return;
			}
		}
		if (!mEventID.isNull())
		{
			// Delete from the display calendar
			KAlarm::deleteDisplayEvent(KCalEvent::uid(mEventID, KCalEvent::DISPLAYING));
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
	kDebug(5950) << "MessageWin::slotShowKMailMessage()\n";
	if (!mKMailSerialNumber)
		return;
	QString err = KAlarm::runKMail(false);
	if (!err.isNull())
	{
		KMessageBox::sorry(this, err);
		return;
	}
	org::kde::kmail::kmail kmail(KMAIL_DBUS_SERVICE, KMAIL_DBUS_PATH, QDBusConnection::sessionBus());
	QDBusReply<bool> reply = kmail.showMail((qulonglong)mKMailSerialNumber, QString());
	if (!reply.isValid())
		kError(5950) << "MessageWin::slotShowKMailMessage(): kmail D-Bus call failed: " << reply.error().message() << endl;
	else if (!reply.value())
		KMessageBox::sorry(this, i18n("Unable to locate this email in KMail"));
}

/******************************************************************************
*  Called when the Edit... button is clicked.
*  Displays the alarm edit dialog.
*/
void MessageWin::slotEdit()
{
	EditAlarmDlg editDlg(false, i18n("Edit Alarm"), this, &mEvent, EditAlarmDlg::RES_IGNORE);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		AlarmResource* resource;
		editDlg.getEvent(event, resource);
		resource = mResource;

		// Update the displayed lists and the calendar file
		KAlarm::UpdateStatus status;
		if (AlarmCalendar::resources()->event(mEventID))
		{
			// The old alarm hasn't expired yet, so replace it
			status = KAlarm::modifyEvent(mEvent, event, &editDlg);
			Undo::saveEdit(mEvent, event, resource);
		}
		else
		{
			// The old event has expired, so simply create a new one
			status = KAlarm::addEvent(event, resource, &editDlg);
			Undo::saveAdd(event, resource);
		}

		if (status == KAlarm::UPDATE_KORG_ERR)
			KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::ERR_MODIFY, 1);
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
		mDeferLimit = event.deferralLimit().effectiveDateTime();
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
	mDeferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), KDateTime::currentDateTime(Preferences::timeZone()).addSecs(60),
	                              false, this);
	mDeferDlg->setObjectName("DeferDlg");    // used by LikeBack
	if (mDefaultDeferMinutes > 0)
		mDeferDlg->setDeferMinutes(mDefaultDeferMinutes);
	mDeferDlg->setLimit(mEventID);
	if (!Preferences::modalMessages())
		lower();
	if (mDeferDlg->exec() == QDialog::Accepted)
	{
		DateTime dateTime  = mDeferDlg->getDateTime();
		int      delayMins = mDeferDlg->deferMinutes();
		// Fetch the up-to-date alarm from the calendar. Note that it could have
		// changed since it was displayed.
		const Event* kcalEvent = mEventID.isNull() ? 0 : AlarmCalendar::resources()->event(mEventID);
		if (kcalEvent)
		{
			// The event still exists in the active calendar
			KAEvent event(kcalEvent);
			bool repeat = event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
			event.setDeferDefaultMinutes(delayMins);
			KAlarm::updateEvent(event, mDeferDlg, true, !repeat);
			if (event.deferred())
				mNoPostAction = true;
		}
		else
		{
			// Try to retrieve the event from the displaying or archive calendars
			AlarmResource* resource = 0;
			KAEvent event;
			bool showEdit, showDefer;
			if (!retrieveEvent(event, resource, showEdit, showDefer))
			{
				// The event doesn't exist any more !?!, so recurrence data,
				// flags, and more, have been lost.
				KMessageBox::error(this, QString::fromLatin1("%1\n%2").arg(i18n("Cannot defer alarm:")).arg(i18n("Alarm not found")));
				raise();
				delete mDeferDlg;
				mDeferDlg = 0;
				mDeferButton->setEnabled(false);
				mEditButton->setEnabled(false);
				return;
			}
			event.defer(dateTime, (mAlarmType & KAAlarm::REMINDER_ALARM), true);
			event.setDeferDefaultMinutes(delayMins);
			// Add the event back into the calendar file, retaining its ID
			// and not updating KOrganizer
			KAlarm::addEvent(event, resource, mDeferDlg, KAlarm::USE_EVENT_ID);
			if (event.deferred())
				mNoPostAction = true;
			event.setCategory(KCalEvent::ARCHIVED);
			KAlarm::deleteEvent(event, false);
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
