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


/**
 *  The TimeEdit class provides a widget to enter a time of day in hours and minutes,
 *  using a 12- or 24-hour clock according to the user's locale settings.
 *
 *  It displays a TimeSpinBox widget to enter hours and minutes. If a 12-hour clock is
 *  being used, it also displays a combo box to select am or pm.
 *
 *  TimeSpinBox displays a spin box with two pairs of spin buttons, one for hours and
 *  one for minutes. It provides accelerated stepping using the spin buttons, when the
 *  shift key is held down (inherited from SpinBox2). The default shift steps are 5
 *  minutes and 6 hours.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @short Widget to enter a time of day.
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class TimeEdit : public QHBox
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		explicit TimeEdit(QWidget* parent = 0, const char* name = 0);
		/** Returns true if the widget is read only. */
		bool          isReadOnly() const           { return mReadOnly; }
		/** Sets whether the spin box is read-only for the user. If read-only,
		 *  the time cannot be edited and the spin buttons and am/pm combo box
		 *  are inactive.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void          setReadOnly(bool readOnly);
		/** Returns true if the spin box contains a valid value. */
		bool          isValid() const;
		/** Sets whether the edit value is valid.
		 *  If newly invalid, the value is displayed as asterisks.
		 *  If newly valid, the value is set to the minimum value.
		 *  @param valid True to set the value valid, false to set it invalid.
		 */
		void          setValid(bool valid);
		/** Returns the entered time as a value in minutes. */
		int           value() const;
		/** Returns the entered time as a QTime value. */
		QTime         time() const                 { int m = value();  return QTime(m/60, m%60); }
		/** Returns true if it is possible to step the value from the highest value to the lowest value and vice versa. */
		bool          wrapping() const;
		/** Sets whether it is possible to step the value from the highest value to the lowest value and vice versa.
		 *  @param on True to enable wrapping, else false.
		 */
		void          setWrapping(bool on);
		/** Returns the minimum value of the spin box in minutes. */
		int           minValue() const;
		/** Returns the maximum value of the spin box in minutes. */
		int           maxValue() const;
		/** Returns the maximum value of the spin box as a QTime value. */
		QTime         maxTime() const              { int mv = maxValue();  return QTime(mv/60, mv%60); }
		/** Sets the minimum value of the spin box. */
		void          setMinValue(int minutes);
		/** Sets the maximum value of the spin box. */
		void          setMaxValue(int minutes);
		/** Sets the maximum value of the spin box. */
		void          setMaxValue(const QTime& time)  { setMaxValue(time.hour()*60 + time.minute()); }
	public slots:
		/** Sets the value of the spin box. */
		virtual void  setValue(int minutes);
		/** Sets the value of the spin box. */
		void          setValue(const QTime& t)     { setValue(t.hour()*60 + t.minute()); }
	signals:
		/** This signal is emitted every time the value of the spin box changes
		 *  (for whatever reason).
		 *  @param minutes The new value.
		 */
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
