/*
 *  timespinbox.h  -  time spinbox widget
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include <qdatetime.h>
#include "spinbox2.h"


class TimeSpinBox : public SpinBox2
{
		Q_OBJECT
	public:
		TimeSpinBox(bool use24hour, QWidget* parent = 0, const char* name = 0);
		TimeSpinBox(int minMinute, int maxMinute, QWidget* parent = 0, const char* name = 0);
		bool            isValid() const;
		QTime           time() const;
		void            setMaxValue(int minutes)     { SpinBox2::setMaxValue(minutes); }
		void            setMaxValue(const QTime& t)  { SpinBox2::setMaxValue(t.hour()*60 + t.minute()); }
		QTime           maxTime() const              { int mv = maxValue();  return QTime(mv/60, mv%60); }
		void            setValid(bool);
		static QString  shiftWhatsThis();
	public slots:
		virtual void    setValue(int minutes);
		void            setValue(const QTime& t)     { setValue(t.hour()*60 + t.minute()); }
		virtual void    stepUp();
		virtual void    stepDown();
	protected:
		virtual QString mapValueToText(int v);
		virtual int     mapTextToValue(bool* ok);
	private slots:
		void            slotValueChanged(int value);
	private:
		class TimeValidator;
		TimeValidator*  mValidator;
		int             mMinimumValue;
		bool            m12Hour;             // use 12-hour clock
		bool            mPm;                 // use PM for manually entered values (with 12-hour clock)
		bool            mInvalid;            // value is currently invalid (asterisks)
		bool            mEnteredSetValue;    // to prevent infinite recursion in setValue()
};

#endif // TIMESPINBOX_H
