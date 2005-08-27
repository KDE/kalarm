/*
 *  pickfileradio.h  -  radio button with an associated file picker
 *  Program:  kalarm
 *  Copyright (C) 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef PICKFILERADIO_H
#define PICKFILERADIO_H

/** @file pickfileradio.h - radio button with an associated file picker */

#include "radiobutton.h"

class QPushButton;
class LineEdit;

/**
 *  @short Radio button with associated file picker controls.
 *
 *  The PickFileRadio class is a radio button with an associated button to choose
 *  a file, and an optional file name edit box. Its purpose is to ensure that while
 *  the radio button is selected, the chosen file name is never blank.
 *
 *  To achieve this, whenever the button is newly selected and the
 *  file name is currently blank, the file picker dialogue is displayed to choose a
 *  file. If the dialogue exits without a file being chosen, the radio button selection
 *  is reverted to the previously selected button in the parent button group.
 *
 *  The class handles the activation of the file picker dialogue (via a virtual method
 *  which must be supplied by deriving a class from this one). It also handles all
 *  enabling and disabling of the browse button and edit box when the enable state of
 *  the radio button is changed, and when the radio button selection changes.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class PickFileRadio : public RadioButton
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param button Push button to invoke the file picker dialogue.
		 *  @param edit File name edit widget, or null if there is none.
		 *  @param text Radio button's text.
		 *  @param parent Button group which is to be the parent object for the radio button.
		 *  @param name The name of this widget.
		 */
		PickFileRadio(QPushButton* button, LineEdit* edit, const QString& text, QButtonGroup* parent, const char* name = 0);
		/** Constructor.
		 *  The init() method must be called before the widget can be used.
		 *  @param text Radio button's text.
		 *  @param parent Button group which is to be the parent object for the radio button.
		 *  @param name The name of this widget.
		 */
		PickFileRadio(const QString& text, QButtonGroup* parent, const char* name = 0);
		/** Initialises the widget.
		 *  @param button Push button to invoke the file picker dialogue.
		 *  @param edit File name edit widget, or null if there is none.
		 */
		void            init(QPushButton* button, LineEdit* edit = 0);
		/** Sets whether the radio button and associated widgets are read-only for the user.
		 *  If read-only, their states cannot be changed by the user.
		 *  @param readOnly True to set the widgets read-only, false to set them read-write.
		 */
		virtual void    setReadOnly(bool readOnly);
		/** Chooses a file, for example by displaying a file selection dialogue.
		 *  This method is called when the push button is clicked - the client
		 *  should not activate a file selection dialogue directly.
		 *  @return Selected file name, or QString::null if no selection made.
		 */
		virtual QString pickFile() = 0;
		/** Notifies the widget of the currently selected file name.
		 *  This should only be used when no file name edit box is used.
		 *  It should be called to initialise the widget's data, and also any time the file
		 *  name is changed without using the push button.
		 */
		void            setFile(const QString& file);
		/** Returns the currently selected file name. */
		QString         file() const;
		/** Returns the associated file name edit widget, or null if none. */
		LineEdit*       fileEdit() const    { return mEdit; }
		/** Returns the associated file browse push button. */
		QPushButton*    pushButton() const  { return mButton; }

	public slots:
		/** Enables or disables the radio button, and adjusts the enabled state of the
		 *  associated browse button and file name edit box.
		 */
		virtual void    setEnabled(bool);

	private slots:
		void          slotSelectionChanged(int id);
		void          slotPickFile();
		void          setLastId();

	private:
		bool          pickFileIfNone();

		QButtonGroup* mGroup;     // button group which radio button is in
		LineEdit*     mEdit;      // file name edit box, or null if none
		QPushButton*  mButton;    // push button to pick a file
		QString       mFile;      // saved file name (if mEdit is null)
		int           mLastId;    // previous radio button selected
		bool          mRevertId;  // true to revert to the previous radio button selection
};

#endif // PICKFILERADIO_H
