/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include <kdialog.h>

#include <msgevent.h>
using namespace KCal;


/**
 * MessageWin: A dialog to display an alarm message
 */
class MessageWin : public KDialog
{
		Q_OBJECT
	public:
		explicit MessageWin(const MessageEvent&, QWidget* parent = 0, bool delete_event = true);
		~MessageWin();
		virtual void paintEvent(QPaintEvent*);

	private:
		QString           eventID;
		QString           audioFile;
		bool              beep;
		QPushButton*      buttonOK;
		bool              deleteEvent;  // true to delete event after message has been displayed
		bool              shown;        // true once the window has been displayed
};

#endif // MESSAGEWIN_H
