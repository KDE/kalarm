/*
 *  timeedit.h  -  time-of-day edit widget, with AM/PM shown depending on locale
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef TIMEEDIT_H
#define TIMEEDIT_H

#include <qdatetime.h>
#include <qhbox.h>

class ComboBox;
class TimeSpinBox;


class TimeEdit : public QHBox
{
		Q_OBJECT
	public:
		explicit TimeEdit(QWidget* parent = 0, const char* name = 0);
		bool          isReadOnly() const           { return mReadOnly; }
		void          setReadOnly(bool);
		bool          isValid() const;
		void          setValid(bool);
		int           value() const;               // time in minutes
		QTime         time() const                 { int m = value();  return QTime(m/60, m%60); }
		bool          wrapping() const;
		void          setWrapping(bool on);
		int           minValue() const;
		int           maxValue() const;
		QTime         maxTime() const              { int mv = maxValue();  return QTime(mv/60, mv%60); }
		void          setMinValue(int minutes);
		void          setMaxValue(int minutes);
		void          setMaxValue(const QTime& t)  { setMaxValue(t.hour()*60 + t.minute()); }
	public slots:
		virtual void  setValue(int minutes);
		void          setValue(const QTime& t)     { setValue(t.hour()*60 + t.minute()); }
	signals:
		void          valueChanged(int minutes);

	private slots:
		void          slotValueChanged(int);
		void          slotAmPmChanged(int item);
	private:
		void          setAmPmCombo(int am, int pm);

		TimeSpinBox*  mSpinBox;
		ComboBox*     mAmPm;
		int           mAmIndex;            // mAmPm index to "am", or -1 if none
		int           mPmIndex;            // mAmPm index to "pm", or -1 if none
		bool          mReadOnly;           // the widget is read only
};

#endif // TIMEEDIT_H
