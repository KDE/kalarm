/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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

#include <qfile.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qlabel.h>
#include <qwhatsthis.h>

#include <kstandarddirs.h>
#include <kaction.h>
#include <kstdguiitem.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdialog.h>
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
#include "prefsettings.h"
#include "deferdlg.h"
#include "messagewin.moc"


static const int MAX_LINE_LENGTH = 80;    // maximum width (in characters) to try to display for a file

QPtrList<MessageWin> MessageWin::windowList;


/******************************************************************************
*  Construct the message window for the specified alarm.
*  Other alarms in the supplied event may have been updated by the caller, so
*  the whole event needs to be stored for updating the calendar file when it is
*  displayed.
*/
MessageWin::MessageWin(const KAlarmEvent& evnt, const KAlarmAlarm& alarm, bool reschedule_event, bool allowDefer)
	: MainWindowBase(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose | WGroupLeader | WStyle_ContextHelp),
	  event(evnt),
	  message(alarm.cleanText()),
	  font(theApp()->settings()->messageFont()),
	  colour(alarm.colour()),
	  dateTime(alarm.repeatAtLogin() ? evnt.firstAlarm().dateTime() : alarm.dateTime()),
	  eventID(evnt.id()),
	  audioFile(evnt.audioFile()),
	  alarmID(alarm.id()),
	  flags(alarm.flags()),
	  beep(alarm.beep()),
	  confirmAck(event.confirmAck()),
	  dateOnly(event.anyTime()),
	  type(alarm.type()),
	  noDefer(!allowDefer || alarm.repeatAtLogin()),
	  deferButton(0L),
	  restoreHeight(0),
	  rescheduleEvent(reschedule_event),
	  shown(false),
	  deferClosing(false)
{
	kdDebug(5950) << "MessageWin::MessageWin(event)" << endl;
	setAutoSaveSettings(QString::fromLatin1("MessageWin"));     // save window sizes etc.
	QSize size = initView();
	if (type == KAlarmAlarm::FILE  &&  errorMsg.isNull())
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
MessageWin::MessageWin(const QString& errmsg, const KAlarmEvent& evnt, const KAlarmAlarm& alarm, bool reschedule_event)
	: MainWindowBase(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose | WGroupLeader | WStyle_ContextHelp),
	  event(evnt),
	  message(alarm.cleanText()),
	  font(theApp()->settings()->messageFont()),
	  colour(Qt::white),
	  dateTime(alarm.repeatAtLogin() ? evnt.firstAlarm().dateTime() : alarm.dateTime()),
	  eventID(evnt.id()),
	  audioFile(evnt.audioFile()),
	  alarmID(alarm.id()),
	  flags(alarm.flags()),
	  beep(false),
	  confirmAck(evnt.confirmAck()),
	  dateOnly(evnt.anyTime()),
	  type(alarm.type()),
	  errorMsg(errmsg),
	  noDefer(true),
	  deferButton(0L),
	  restoreHeight(0),
	  rescheduleEvent(reschedule_event),
	  shown(false),
	  deferClosing(false)
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
	: MainWindowBase(0L, "MessageWin", WStyle_StaysOnTop | WDestructiveClose),
	  rescheduleEvent(false),
	  shown(true),
	  deferClosing(false)
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
}

/******************************************************************************
*  Construct the message window.
*/
QSize MessageWin::initView()
{
	setCaption(i18n("Message"));
	QWidget* topWidget = new QWidget(this, "messageWinTop");
	setCentralWidget(topWidget);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget, KDialog::marginHint(), KDialog::spacingHint());

	if (dateTime.isValid())
	{
		// Alarm date/time
		QLabel* label = new QLabel(topWidget);
		label->setText(dateOnly ? KGlobal::locale()->formatDate(dateTime.date(), true)
		                        : KGlobal::locale()->formatDateTime(dateTime));
		label->setFrameStyle(QFrame::Box | QFrame::Raised);
		label->setFixedSize(label->sizeHint());
		topLayout->addWidget(label, 0, Qt::AlignHCenter);
		QWhatsThis::add(label,
		      i18n("The scheduled date/time for the message (as opposed to the actual time of display)."));
	}

	QLabel* fileCommandLabel = 0;
	if (type == KAlarmAlarm::FILE  ||  type == KAlarmAlarm::COMMAND)
	{
		// Display the file name or command
		fileCommandLabel = new QLabel(message, topWidget);
		fileCommandLabel->setFrameStyle(QFrame::Box | QFrame::Raised);
		fileCommandLabel->setFixedSize(fileCommandLabel->sizeHint());
		topLayout->addWidget(fileCommandLabel, 0, Qt::AlignHCenter);
	}
	switch (type)
	{
		case KAlarmAlarm::COMMAND:
		{
			QWhatsThis::add(fileCommandLabel, i18n("The command to execute"));
			break;
		}
		case KAlarmAlarm::FILE:
		{
			QWhatsThis::add(fileCommandLabel, i18n("The file whose contents are displayed below"));

			// Display contents of file
			bool opened = false;
			bool dir = false;
			QString tmpFile;
			KURL url(message);
			if (KIO::NetAccess::download(url, tmpFile))
			{
				QFile qfile(tmpFile);
				QFileInfo info(qfile);
				if (!(dir = info.isDir())
				&&  qfile.open(IO_ReadOnly|IO_Translate))
				{
					opened = true;
					QTextEdit* view = new QTextEdit(topWidget, "fileContents");
					view->setReadOnly(true);
					topLayout->addWidget(view);
					QFontMetrics fm = view->fontMetrics();
					QString line;
					int n;
					for (n = 0;  qfile.readLine(line, 4096) > 0;  ++n)
					{
						int nl = line.find('\n');
						if (nl >= 0)
							line = line.left(nl);
						view->append(line);
					}
					qfile.close();
					view->setMinimumSize(view->sizeHint());

					// Set the default size to square, max 20 lines.
					// Note that after the first file has been displayed, this size
					// is overridden by the user-set default stored in the config file.
					// So there is no need to calculate an accurate size.
					int h = fm.lineSpacing() * (n <= 20 ? n : 20) + 2*view->frameWidth();
					view->resize(QSize(h, h).expandedTo(view->sizeHint()));
					QWhatsThis::add(view, i18n("The contents of the file to be displayed"));
				}
				KIO::NetAccess::removeTempFile(tmpFile);
			}
			if (!opened)
			{
				// File couldn't be opened
				bool exists = KIO::NetAccess::exists(url);
				errorMsg = dir ? i18n("File is a directory") : exists ? i18n("Failed to open file") : i18n("File not found");
			}
			break;
		}
#ifdef KALARM_EMAIL
		case KAlarmAlarm::EMAIL:
		{
			// Display the email addresses and subject
			QFrame* frame = new QFrame(topWidget);
			frame->setFrameStyle(QFrame::Box | QFrame::Raised);
			QWhatsThis::add(frame, i18n("The email to send"));
			topLayout->addWidget(frame, 0, Qt::AlignHCenter);
			QGridLayout* grid = new QGridLayout(frame, 2, 2, KDialog::marginHint(), KDialog::spacingHint());

			QLabel* label = new QLabel(i18n("To:"), frame);
			label->setFixedSize(label->sizeHint());
			grid->addWidget(label, 0, 0, Qt::AlignLeft);
			label = new QLabel(emailAddresses, frame);
			label->setFixedSize(label->sizeHint());
			grid->addWidget(label, 0, 1, Qt::AlignLeft);

			label = new QLabel(i18n("Subject:"), frame);
			label->setFixedSize(label->sizeHint());
			grid->addWidget(label, 1, 0, Qt::AlignLeft);
			label = new QLabel(emailSubject, frame);
			label->setFixedSize(label->sizeHint());
			grid->addWidget(label, 1, 1, Qt::AlignLeft);
			break;
		}
#endif
		case KAlarmAlarm::MESSAGE:
		default:
		{
			// Message label
			QLabel* label = new QLabel(message, topWidget);
			label->setFont(font);
			label->setPalette(QPalette(colour, colour));
			label->setFixedSize(label->sizeHint());
			QWhatsThis::add(label, i18n("The alarm message"));
			int lineSpacing = label->fontMetrics().lineSpacing();
			int vspace = lineSpacing/2 - marginKDE2;
			int hspace = lineSpacing - marginKDE2 - KDialog::marginHint();
			topLayout->addSpacing(vspace);
			QBoxLayout* layout = new QHBoxLayout(topLayout);
			layout->addSpacing(hspace);
			layout->addWidget(label, 0, Qt::AlignHCenter);
			layout->addSpacing(hspace);
			topLayout->addSpacing(vspace);
			break;
		}
	}
	if (errorMsg.isNull())
		topWidget->setBackgroundColor(colour);
	else
	{
		setCaption(i18n("Error"));
		QHBoxLayout* layout = new QHBoxLayout(topLayout);
		layout->setMargin(2*KDialog::marginHint());
		layout->addStretch();
		QLabel* label = new QLabel(topWidget);
		label->setPixmap(DesktopIcon("error"));
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignRight);
		label = new QLabel(errorMsg, topWidget);
		label->setFixedSize(label->sizeHint());
		layout->addWidget(label, 0, Qt::AlignLeft);
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
	connect(button, SIGNAL(clicked()), theApp(), SLOT(displayMainWindow()));
	grid->addWidget(button, 0, 3, AlignHCenter);
	QWhatsThis::add(button, i18n("Activate %1").arg(kapp->aboutData()->programName()));

	// Set the button sizes
	QSize minbutsize = okButton->sizeHint();
	if (!noDefer)
	{
		minbutsize = minbutsize.expandedTo(deferButton->sizeHint());
		deferButton->setFixedSize(minbutsize);
	}
	okButton->setFixedSize(minbutsize);

	topLayout->activate();
	QSize size(minbutsize.width()*3, topLayout->sizeHint().height());
	setMinimumSize(size);

	KWin::setState(winId(), NET::Modal | NET::Sticky | NET::StaysOnTop);
	KWin::setOnAllDesktops(winId(), true);
	return size;
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
		config->writeEntry(QString::fromLatin1("AlarmID"), alarmID);
		config->writeEntry(QString::fromLatin1("Message"), message);
		config->writeEntry(QString::fromLatin1("Type"), (errorMsg.isNull() ? type : -1));
		config->writeEntry(QString::fromLatin1("Font"), font);
		config->writeEntry(QString::fromLatin1("Colour"), colour);
		config->writeEntry(QString::fromLatin1("ConfirmAck"), confirmAck);
		if (dateTime.isValid())
		{
			config->writeEntry(QString::fromLatin1("Time"), dateTime);
			config->writeEntry(QString::fromLatin1("DateOnly"), dateOnly);
		}
		config->writeEntry(QString::fromLatin1("Height"), height());
		config->writeEntry(QString::fromLatin1("NoDefer"), noDefer);
	}
	else
		config->writeEntry(QString::fromLatin1("AlarmID"), -1);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MessageWin::readProperties(KConfig* config)
{
	eventID       = config->readEntry(QString::fromLatin1("EventID"));
	alarmID       = config->readNumEntry(QString::fromLatin1("AlarmID"));
	message       = config->readEntry(QString::fromLatin1("Message"));
	int t         = config->readNumEntry(QString::fromLatin1("Type"));    // don't copy straight into an enum value in case -1 gets lruncated
	if (t < 0)
		errorMsg = "";       // set non-null
	type          = KAlarmAlarm::Type(t);
	font          = config->readFontEntry(QString::fromLatin1("Font"));
	colour        = config->readColorEntry(QString::fromLatin1("Colour"));
	confirmAck    = config->readBoolEntry(QString::fromLatin1("ConfirmAck"));
	QDateTime invalidDateTime;
	dateTime      = config->readDateTimeEntry(QString::fromLatin1("Time"), &invalidDateTime);
	dateOnly      = config->readBoolEntry(QString::fromLatin1("DateOnly"));
	restoreHeight = config->readNumEntry(QString::fromLatin1("Height"));
	noDefer       = config->readBoolEntry(QString::fromLatin1("NoDefer"));
	if (errorMsg.isNull()  &&  alarmID > 0)
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
	return 0L;
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
void MessageWin::repeat()
{
	const Event* kcalEvent = eventID.isNull() ? 0L : theApp()->getCalendar().event(eventID);
	if (kcalEvent)
	{
		raise();
		playAudio();
		KAlarmEvent event(*kcalEvent);
		theApp()->rescheduleAlarm(event, alarmID);
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
			theApp()->rescheduleAlarm(event, alarmID);
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
		if (type == KAlarmAlarm::FILE  &&  errorMsg.isNull())
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
//kdDebug()<<"closeEvent: defer closing="<<(int)deferClosing<<", confack="<<(int)confirmAck<<endl;
	if (confirmAck  &&  !deferClosing  &&  !theApp()->sessionClosingDown())
	{
		// Ask for confirmation of acknowledgement. Use warningYesNo() because its default is No.
		if (KMessageBox::warningYesNo(this, i18n("Do you really want to acknowledge this alarm?"),
		                                       i18n("Acknowledge Alarm"), i18n("&Acknowledge"), KStdGuiItem::cancel().text())
		    != KMessageBox::Yes)
		{
			ce->ignore();
			return;
		}
	}
	MainWindowBase::closeEvent(ce);
}

/******************************************************************************
*  Called when the Defer... button is clicked.
*  Displays the defer message dialog.
*/
void MessageWin::slotDefer()
{
	KAlarmEvent event;
	QDateTime endTime;
	// Get the event being deferred. It will only still exist if repetitions are outstanding.
	const Event* kcalEvent = eventID.isNull() ? 0L : theApp()->getCalendar().event(eventID);
	if (kcalEvent)
	{
		// It's a repeated alarm which may still exist in the calendar file.
		// Don't allow it to be deferred past its next occurrence.
		event.set(*kcalEvent);
		event.nextOccurrence(QDateTime::currentDateTime(), endTime);
	}
	DeferAlarmDlg* deferDlg = new DeferAlarmDlg(i18n("Defer Alarm"), QDateTime::currentDateTime().addSecs(60),
						    endTime, false, this, "deferDlg");
	if (deferDlg->exec() == QDialog::Accepted)
	{
		QDateTime dateTime = deferDlg->getDateTime();
		if (kcalEvent)
		{
			// It's a repeated alarm which may still exist in the calendar file.
			event.defer(dateTime);
			theApp()->updateEvent(event, 0L);
		}
		else
		{
			// The event doesn't exist any more, so create a new one
			KAlarmEvent event(dateTime, message, colour, type, flags);
			event.setAudioFile(audioFile);
			theApp()->addEvent(event, 0L);
		}
		if (theApp()->runInSystemTray())
		{
			// Alarms are displayed only if the system tray icon is running,
			// so start it if necessary so that the deferred alarm will be shown.
			theApp()->displayTrayIcon(true);
		}
		deferClosing = true;   // allow window to close without confirmation prompt
		close();
	}
}
