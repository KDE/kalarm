/*
 *  messagewin.cpp  -  displays an alarm message
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qwhatsthis.h>

#include <kstddirs.h>
#include <klocale.h>
#include <knotifyclient.h>
#include <kaudioplayer.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "messagewin.h"
#include "messagewin.moc"


/******************************************************************************
*  Construct the message window.
*/
MessageWin::MessageWin(const MessageEvent& event, QWidget* parent, bool delete_event)
	: KDialog(parent, "MessageWin", false, WStyle_StaysOnTop | WDestructiveClose),
	  eventID(event.VUID()),
	  audioFile(event.alarm()->audioFile()),
	  beep(event.beep()),
	  deleteEvent(delete_event),
	  shown(false)
{
	kdDebug() << "MessageWin::MessageWin()" << endl;
	setCaption(i18n("Message"));
	setBackgroundColor(event.colour());

	QVBoxLayout* topLayout = new QVBoxLayout(this, marginHint(), spacingHint());

	// Alarm date/time

	QLabel* label = new QLabel(this);
	label->setText(KGlobal::locale()->formatDateTime(event.dateTime()));
	label->setFrameStyle(QFrame::Box | QFrame::Raised);
	label->setFixedSize(label->sizeHint());
	topLayout->addWidget(label);
	QWhatsThis::add(label,
	      i18n("The scheduled date/time for the message\n"
	           "(as opposed to the actual time of display)."));

	// Message label

	label = new QLabel(this);
	label->setText(event.message());
	label->setFont(theApp()->generalSettings()->messageFont());
	label->setPalette(QPalette(event.colour(), event.colour()));
	label->setFixedSize(label->sizeHint());
	int spacing = label->fontMetrics().lineSpacing()/2 - spacingHint();
	topLayout->addSpacing(spacing);
	topLayout->addWidget(label);
	topLayout->addSpacing(spacing);

	// OK button

	QPushButton* but = new QPushButton(i18n("ABCDEF"), this);
	QSize size = but->sizeHint();
	delete but;

	but = new QPushButton(i18n("&OK"), this);
	but->setFixedSize(size);
	but->setDefault(true);
	connect(but, SIGNAL(clicked()), SLOT(reject()));
	topLayout->addWidget(but);

	topLayout->activate();
	setMinimumWidth(size.width()*3);
}

/******************************************************************************
* Destructor.
* Notifies the application that the window will now be closed.
*/
MessageWin::~MessageWin()
{
	theApp()->deleteWindow(this);
}

/******************************************************************************
*  Display the message window.
*  The first time, output any required audio notification.
*/
void MessageWin::paintEvent(QPaintEvent* pe)
{
	KDialog::paintEvent(pe);
	if (!shown)
	{
		if (beep)
		{
			// Beep using two methods, in case the sound card/speakers are switched off or not working
			KNotifyClient::beep();     // beep through the sound card & speakers
			QApplication::beep();      // beep through the internal speaker
		}
		if (!audioFile.isEmpty())
			KAudioPlayer::play(audioFile.latin1());
		if (deleteEvent)
			KAlarmApp::getInstance()->deleteMessage(eventID);
		shown = true;
	}
}
