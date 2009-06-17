/*
 *  pickfileradio.h  -  radio button with an associated file picker
 *  Program:  kalarm
 *  Copyright © 2005,2009 by David Jarvie <djarvie@kde.org>
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
class ButtonGroup;
class LineEdit;

/**
 *  @short Radio button with associated file picker controls.
 *
 *  The PickFileRadio class is a radio button with an associated button to choose
 *  a file, and an optional file name edit box. Its purpose is to ensure that while
 *  the radio button is selected, the chosen file name is never blank.
 *
 *  To achieve this, whenever the button is newly selected and the
 *  file name is currently blank, the file picker dialog is displayed to choose a
 *  file. If the dialog exits without a file being chosen, the radio button selection
 *  is reverted to the previously selected button in the parent button group.
 *
 *  The class handles the activation of the file picker dialog (via a virtual method
 *  which must be supplied by deriving a class from this one). It also handles all
 *  enabling and disabling of the browse button and edit box when the enable state of
 *  the radio button is changed, and when the radio button selection changes.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class PickFileRadio : public RadioButton
{
		Q_OBJECT
	public:
		/** Constructor.
		 *  @param button Push button to invoke the file picker dialog.
		 *  @param edit File name edit widget, or null if there is none.
		 *  @param text Radio button's text.
		 *  @param parent Button group which is to be the parent object for the radio button.
		 */
		PickFileRadio(QPushButton* button, LineEdit* edit, const QString& text, ButtonGroup* group, QWidget* parent);
		/** Constructor.
		 *  The init() method must be called before the widget can be used.
		 *  @param text Radio button's text.
		 *  @param parent Button group which is to be the parent object for the radio button.
		 */
		PickFileRadio(const QString& text, ButtonGroup* group, QWidget* parent);
		/** Initialises the widget.
		 *  @param button Push button to invoke the file picker dialog.
		 *  @param edit File name edit widget, or null if there is none.
		 */
		void            init(QPushButton* button, LineEdit* edit = 0);
		/** Sets whether the radio button and associated widgets are read-only for the user.
		 *  If read-only, their states cannot be changed by the user.
		 *  @param readOnly True to set the widgets read-only, false to set them read-write.
		 */
		virtual void    setReadOnly(bool readOnly);
		/** Chooses a file, for example by displaying a file selection dialog.
		 *  This method is called when the push button is clicked - the client
		 *  should not activate a file selection dialog directly.
		 *  @return Selected file name, or QString() if no selection made.
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
		void          slotSelectionChanged(QAbstractButton*);
		QString       slotPickFile();
		void          setLastButton();

	private:
		bool          pickFileIfNone();

		ButtonGroup*     mGroup;         // button group which radio button is in
		LineEdit*        mEdit;          // file name edit box, or null if none
		QPushButton*     mButton;        // push button to pick a file
		QString          mFile;          // saved file name (if mEdit is null)
		QAbstractButton* mLastButton;    // previous radio button selected
		bool             mRevertButton;  // true to revert to the previous radio button selection
};

#endif // PICKFILERADIO_H
