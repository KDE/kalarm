/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
#include <kaudioplayer.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "preferences.h"
#include "deferdlg.h"
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
		virtual QSize sizeHint() const
		{ return QSize(contentsWidth(), contentsHeight() + horizontalScrollBar()->height()); }
};


class TextMimeSourceFactory : public QMimeSourceFactory
{
	public:
		TextMimeSourceFactory(const QString& absPath, KTextBrowser*);
		virtual ~TextMimeSourceFactory();
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


QPtrList<MessageWin> MessageWin::windowList;


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAlarmEvent& evnt, const KAlarmAlarm& alarm, bool reschedule_event, bool allowDefer)
	: MainWindowBase(0, "MessageWin", WFLAGS | Qt::WGroupLeader | Qt::WStyle_ContextHelp
	                                         | (Preferences::instance()->modalMessages() ? 0 : Qt::WX11BypassWM)),
	  mEvent(evnt),
	  message(alarm.cleanText()),
	  font(evnt.font()),
	  mBgColour(evnt.bgColour()),
	  mFgColour(evnt.fgColour()),
	  mDateTime((alarm.type() & KAlarmAlarm::REMINDER_ALARM) ? evnt.mainDateTime() : alarm.dateTime()),
	  eventID(evnt.id()),
	  mAlarmType(alarm.type()),
	  flags(alarm.flags()),
	  beep(evnt.beep()),
	  confirmAck(evnt.confirmAck()),
	  action(alarm.action()),
	  noDefer(!allowDefer || alarm.repeatAtLogin()),
	  deferButton(0),
	  restoreHeight(0),
	  rescheduleEvent(reschedule_event),
	  shown(false),
	  deferClosing(false),
	  mDeferDlgShowing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWin"));     // save window sizes etc.
	QSize size = initView();
	if (action == KAlarmAlarm::FILE  &&  !mErrorMsgs.count())
		size = theApp()->readConfigWindowSize("FileMessage", size);
	resize(size);
	windowList.append(this);
}

/******************************************************************************
*  Construct the message window for a specified error message.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAlarmEvent& evnt, const KAlarmAlarm& alarm, const QStringList& errmsgs, bool reschedule_event)
	: MainWindowBase(0, "MessageWin", WFLAGS | Qt::WGroupLeader | Qt::WStyle_ContextHelp),
	  mEvent(evnt),
	  message(alarm.cleanText()),
	  font(evnt.font()),
	  mBgColour(Qt::white),
	  mFgColour(Qt::black),
	  mDateTime(alarm.dateTime()),
	  eventID(evnt.id()),
	  mAlarmType(alarm.type()),
	  flags(alarm.flags()),
	  beep(false),
	  confirmAck(evnt.confirmAck()),
	  action(alarm.action()),
	  mErrorMsgs(errmsgs),
	  noDefer(true),
	  deferButton(0),
	  restoreHeight(0),
	  rescheduleEvent(reschedule_event),
	  shown(false),
	  deferClosing(false),
	  mDeferDlgShowing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWin"));     // save window sizes etc.
	QSize size = initView();
	resize(size);
	windowList.append(this);
}

/******************************************************************************
*  Construct the message window for restoration by session management.
*  The window is initialised by readProperties().
*/
MessageWin::MessageWin()
	: MainWindowBase(0, "MessageWin", WFLAGS),
	  rescheduleEvent(false),
	  shown(true),
	  deferClosing(false),
	  mDeferDlgShowing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin()\n";
	windowList.append(this);
}

MessageWin::~MessageWin()
{
	kdDebug(5950) << "MessageWin::~MessageWin()\n";
	for (MessageWin* w = windowList.first();  w;  w = windowList.next())
		if (w == this)
		{
			windowList.remove();
			break;
		}
	if (!windowList.count())
		theApp()->quitIf();
}

/******************************************************************************
*  Construct the message window.
*/
QSize MessageWin::initView()
{
	setCaption((mAlarmType & KAlarmAlarm::REMINDER_ALARM) ? i18n("Reminder") : i18n("Message"));
	QWidget* topWidget = new QWidget(this, "messageWinTop");
	setCentralWidget(topWidget);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget, KDialog::marginHint(), KDialog::spacingHint());

	if (mDateTime.isValid())
	{
		// Alarm date/time
		QLabel* label = new QLabel(topWidget);
		label->setText(mDateTime.isDateOnly()
		               ? KGlobal::locale()->formatDate(mDateTime.date(), true)
		               : KGlobal::locale()->formatDateTime(mDateTime.dateTime()));
		label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		topLayout->addWidget(label, 0, Qt::AlignHCenter);
		QWhatsThis::add(label,
		      i18n("The scheduled date/time for the message (as opposed to the actual time of display)."));
	}

	switch (action)
	{
		case KAlarmAlarm::FILE:
		{
			// Display the file name
			QLabel* label = new QLabel(message, topWidget);
			label->setFrameStyle(QFrame::Box | QFrame::Raised);
			label->setFixedSize(label->sizeHint());
			QWhatsThis::add(label, i18n("The file whose contents are displayed below"));
			topLayout->addWidget(label, 0, Qt::AlignHCenter);

			// Display contents of file
			bool opened = false;
			bool dir = false;
			QString tmpFile;
			KURL url(message);
			if (KIO::NetAccess::download(url, tmpFile))
			{
				QFile qfile(tmpFile);
				QFileInfo info(qfile);
				if (!(dir = info.isDir()))
				{
					opened = true;
					KTextBrowser* view = new KTextBrowser(topWidget, "fileContents");
					TextMimeSourceFactory msf(tmpFile, view);
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
				mErrorMsgs.clear();
				mErrorMsgs += dir ? i18n("File is a folder") : exists ? i18n("Failed to open file") : i18n("File not found");
			}
			break;
		}
		case KAlarmAlarm::EMAIL:
		{
			// Display the email addresses and subject
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
		case KAlarmAlarm::COMMAND:
			break;

		case KAlarmAlarm::MESSAGE:
		default:
		{
			// Message label
			// Using MessageText instead of QLabel allows scrolling and mouse copying
			MessageText* text = new MessageText(message, QString::null, topWidget);
			text->setFrameStyle(QFrame::NoFrame);
			text->setPaper(mBgColour);
			text->setPaletteForegroundColor(mFgColour);
			text->setFont(font);
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
			topLayout->addStretch();
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

	// Close button
	QPushButton* okButton = new QPushButton(KStdGuiItem::close().text(), topWidget);
	// Prevent accidental acknowledgement of the message if the user is typing when the window appears
	okButton->clearFocus();
	okButton->setFocusPolicy(QWidget::ClickFocus);    // don't allow keyboard selection
	connect(okButton, SIGNAL(clicked()), SLOT(close()));
	grid->addWidget(okButton, 0, 1, AlignHCenter);
	QWhatsThis::add(okButton, i18n("Acknowledge the alarm"));

	if (!noDefer)
	{
		// Defer button
		deferButton = new QPushButton(i18n("&Defer..."), topWidget);
		deferButton->setFocusPolicy(QWidget::ClickFocus);    // don't allow keyboard selection
		connect(deferButton, SIGNAL(clicked()), SLOT(slotDefer()));
		grid->addWidget(deferButton, 0, 2, AlignHCenter);
		QWhatsThis::add(deferButton,
		      i18n("Defer the alarm until later.\n"
		           "You will be prompted to specify when the alarm should be redisplayed."));
	}

	// KAlarm button
	KIconLoader iconLoader;
	QPixmap pixmap = iconLoader.loadIcon(QString::fromLatin1(kapp->aboutData()->appName()), KIcon::MainToolbar);
	QPushButton* button = new QPushButton(topWidget);
	button->setPixmap(pixmap);
	button->setFixedSize(button->sizeHint());
	connect(button, SIGNAL(clicked()), SLOT(displayMainWindow()));
	grid->addWidget(button, 0, 3, AlignHCenter);
	QString actKAlarm = i18n("Activate %1").arg(kapp->aboutData()->programName());
	QToolTip::add(button, actKAlarm);
	QWhatsThis::add(button, actKAlarm);

	// Set the button sizes
	QSize minbutsize = okButton->sizeHint();
	if (!noDefer)
	{
		minbutsize = minbutsize.expandedTo(deferButton->sizeHint());
		deferButton->setFixedSize(minbutsize);
	}
	okButton->setFixedSize(minbutsize);

	topLayout->activate();
	QSize size(minbutsize.width()*3, sizeHint().height());
	setMinimumSize(size);

	WId winid = winId();
	unsigned long wstate = (Preferences::instance()->modalMessages() ? NET::Modal : 0) | NET::Sticky | NET::StaysOnTop;
	KWin::setState(winid, wstate);
	KWin::setOnAllDesktops(winid, true);
	return sizeHint();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MessageWin::saveProperties(KConfig* config)
{
	if (shown)
	{
		config->writeEntry(QString::fromLatin1("EventID"), eventID);
		config->writeEntry(QString::fromLatin1("AlarmType"), mAlarmType);
		config->writeEntry(QString::fromLatin1("Message"), message);
		config->writeEntry(QString::fromLatin1("Type"), (mErrorMsgs.count() ? -1 : action));
		config->writeEntry(QString::fromLatin1("Font"), font);
		config->writeEntry(QString::fromLatin1("BgColour"), mBgColour);
		config->writeEntry(QString::fromLatin1("FgColour"), mFgColour);
		config->writeEntry(QString::fromLatin1("ConfirmAck"), confirmAck);
		if (mDateTime.isValid())
		{
			config->writeEntry(QString::fromLatin1("Time"), mDateTime.dateTime());
			config->writeEntry(QString::fromLatin1("DateOnly"), mDateTime.isDateOnly());
		}
		config->writeEntry(QString::fromLatin1("Height"), height());
		config->writeEntry(QString::fromLatin1("NoDefer"), noDefer);
	}
	else
		config->writeEntry(QString::fromLatin1("AlarmType"), KAlarmAlarm::INVALID_ALARM);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being restored.
* Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(KConfig* config)
{
	eventID       = config->readEntry(QString::fromLatin1("EventID"));
	mAlarmType    = KAlarmAlarm::Type(config->readNumEntry(QString::fromLatin1("AlarmType")));
	message       = config->readEntry(QString::fromLatin1("Message"));
	int t         = config->readNumEntry(QString::fromLatin1("Type"));    // don't copy straight into an enum value in case -1 gets truncated
	if (t < 0)
		mErrorMsgs += "";       // set non-null
	action        = KAlarmAlarm::Action(t);
	font          = config->readFontEntry(QString::fromLatin1("Font"));
	mBgColour     = config->readColorEntry(QString::fromLatin1("BgColour"));
	mFgColour     = config->readColorEntry(QString::fromLatin1("FgColour"));
	confirmAck    = config->readBoolEntry(QString::fromLatin1("ConfirmAck"));
	QDateTime invalidDateTime;
	QDateTime dt  = config->readDateTimeEntry(QString::fromLatin1("Time"), &invalidDateTime);
	bool dateOnly = config->readBoolEntry(QString::fromLatin1("DateOnly"));
	mDateTime.set(dt, dateOnly);
	restoreHeight = config->readNumEntry(QString::fromLatin1("Height"));
	noDefer       = config->readBoolEntry(QString::fromLatin1("NoDefer"));
	if (!mErrorMsgs.count()  &&  mAlarmType != KAlarmAlarm::INVALID_ALARM)
		initView();
}

/******************************************************************************
*  Returns the existing message window (if any) which is displaying the event
*  with the specified ID.
*/
MessageWin* MessageWin::findEvent(const QString& eventID)
{
	for (MessageWin* w = windowList.first();  w;  w = windowList.next())
		if (w->eventID == eventID)
			return w;
	return 0;
}

/******************************************************************************
*  Beep and play the audio file, as appropriate.
*/
void MessageWin::playAudio()
{
	if (beep)
	{
		// Beep using two methods, in case the sound card/speakers are switched off or not working
		KNotifyClient::beep();     // beep through the sound card & speakers
		QApplication::beep();      // beep through the internal speaker
	}
	const QString& audioFile = mEvent.audioFile();
	if (!audioFile.isEmpty())
	{
		QString play = audioFile;
		QString file = QString::fromLatin1("file:");
		if (audioFile.startsWith(file))
			play = audioFile.mid(file.length());
		KAudioPlayer::play(QFile::encodeName(play));
	}
}

/******************************************************************************
*  Re-output any required audio notification, and reschedule the alarm in the
*  calendar file.
*/
void MessageWin::repeat(const KAlarmAlarm& alarm)
{
	const Event* kcalEvent = eventID.isNull() ? 0 : theApp()->getCalendar().event(eventID);
	if (kcalEvent)
	{
		mAlarmType = alarm.type();    // store new alarm type for use if it is later deferred
		if (!mDeferDlgShowing  ||  Preferences::instance()->modalMessages())
		{
				raise();
				playAudio();
		}
		KAlarmEvent event(*kcalEvent);
		theApp()->alarmShowing(event, mAlarmType, mDateTime);
	}
}

/******************************************************************************
*  Called when the window is shown.
*  The first time, output any required audio notification, and reschedule or
*  delete the event from the calendar file.
*/
void MessageWin::showEvent(QShowEvent* se)
{
	MainWindowBase::showEvent(se);
	if (!shown)
	{
		playAudio();
		if (rescheduleEvent)
			theApp()->alarmShowing(mEvent, mAlarmType, mDateTime);
		shown = true;
	}
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*/
void MessageWin::resizeEvent(QResizeEvent* re)
{
	if (restoreHeight)
	{
		if (restoreHeight != re->size().height())
		{
			QSize size = re->size();
			size.setHeight(restoreHeight);
			resize(size);
		}
		else if (isVisible())
			restoreHeight = 0;
	}
	else
	{
		if (action == KAlarmAlarm::FILE  &&  !mErrorMsgs.count())
			theApp()->writeConfigWindowSize("FileMessage", re->size());
		MainWindowBase::resizeEvent(re);
	}
}

/******************************************************************************
*  Called when a close event is received.
*  Only quits the application if there is no system tray icon displayed.
*/
void MessageWin::closeEvent(QCloseEvent* ce)
{
	if (confirmAck  &&  !deferClosing  &&  !theApp()->sessionClosingDown())
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
	if (!eventID.isNull())
	{
		// Delete from the display calendar
		theApp()->deleteDisplayEvent(KAlarmEvent::uid(eventID, KAlarmEvent::DISPLAYING));
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
	deferDlg.setLimit(eventID);
	mDeferDlgShowing = true;
	if (!Preferences::instance()->modalMessages())
		lower();
	if (deferDlg.exec() == QDialog::Accepted)
	{
		DateTime dateTime = deferDlg.getDateTime();
		const Event* kcalEvent = eventID.isNull() ? 0 : theApp()->getCalendar().event(eventID);
		if (kcalEvent)
		{
			// The event still exists in the calendar file.
			KAlarmEvent event(*kcalEvent);
			event.defer(dateTime, (mAlarmType & KAlarmAlarm::REMINDER_ALARM), true);
			theApp()->updateEvent(event, 0);
		}
		else
		{
			KAlarmEvent event;
			kcalEvent = theApp()->displayCalendar().event(KAlarmEvent::uid(eventID, KAlarmEvent::DISPLAYING));
			if (kcalEvent)
			{
				event.reinstateFromDisplaying(KAlarmEvent(*kcalEvent));
				event.defer(dateTime, (mAlarmType & KAlarmAlarm::REMINDER_ALARM), true);
			}
			else
			{
				// The event doesn't exist any more !?!, so create a new one
				event.set(dateTime.dateTime(), message, mBgColour, mFgColour, font, (KAlarmEvent::Action)action, flags);
				event.setAudioFile(mEvent.audioFile());
				event.setArchive();
				event.setEventID(eventID);
			}
			// Add the event back into the calendar file, retaining its ID
			theApp()->addEvent(event, 0, true);
			if (kcalEvent)
			{
				event.setUid(KAlarmEvent::EXPIRED);
				theApp()->deleteEvent(event, 0, false, false);
			}
		}
		if (theApp()->wantRunInSystemTray())
		{
			// Alarms are to be displayed only if the system tray icon is running,
			// so start it if necessary so that the deferred alarm will be shown.
			theApp()->displayTrayIcon(true);
		}
		deferClosing = true;   // allow window to close without confirmation prompt
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
	theApp()->displayMainWindowSelected(eventID);
}


/*=============================================================================
= Class TextMimeSourceFactory
* Gets the mime type of a text file from not only its extension (as per
* QMimeSourceFactory), but also from its contents. This allows the detection
* of plain text files without file name extensions.
=============================================================================*/
TextMimeSourceFactory::TextMimeSourceFactory(const QString& absPath, KTextBrowser* view)
	: QMimeSourceFactory(),
	  mMimeType("text/plain"),
	  mLast(0)
{
	view->setMimeSourceFactory(this);
	QString type = KMimeType::findByPath(absPath)->name();
	switch (KAlarmApp::fileType(type))
	{
		case 1:
		case 2:
			mMimeType = type.latin1();
			// fall through to '3'
		case 3:
		default:
			// It's assumed to be a text file
			mTextFile = absPath;
			view->QTextBrowser::setSource(absPath);
			break;

		case 4:
			// It's an image file
			QString text = "<img source=\"";
			text += absPath;
			text += "\">";
			view->setText(text);
			break;
	}
	setFilePath(QFileInfo(absPath).dirPath(true));
}

TextMimeSourceFactory::~TextMimeSourceFactory()
{
	delete mLast;
}

const QMimeSource* TextMimeSourceFactory::data(const QString& abs_name) const
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
