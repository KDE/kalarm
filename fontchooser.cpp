/*
 *  fontchooser.cpp  -  font selection widget
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This module is a modification of the FontChooser class in kfontdialog.cpp,
 *  with extra parameters for color() and setColor().
 */
/*
    $Id$

    Requires the Qt widget libraries, available at no cost at
    http://www.troll.no

    Copyright (C) 1996 Bernd Johannes Wuebben  <wuebben@kde.org>
    Copyright (c) 1999 Preston Brown <pbrown@kde.org>
    Copyright (c) 1999 Mario Weilguni <mweilguni@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <qcombobox.h>
#include <qfile.h>
#include <qfont.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qscrollbar.h>
#include <qstringlist.h>
#include <qfontdatabase.h>

#include <kapp.h>
#include <kcharsets.h>
#include <kconfig.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <qlineedit.h>
#include <klistbox.h>
#include <klocale.h>
#include <kstddirs.h>
#include <kdebug.h>

#include <X11/Xlib.h>

#include "fontchooser.h"
#include "fontchooser.moc"

static int minimumListWidth( const QListBox *list )
{
  int w=0;
  for( uint i=0; i<list->count(); i++ )
  {
    int itemWidth = list->item(i)->width(list);
    w = QMAX(w,itemWidth);
  }
  if( w == 0 ) { w = 40; }
  w += list->frameWidth() * 2;
  w += list->verticalScrollBar()->sizeHint().width();
  return( w );
}

static int minimumListHeight( const QListBox *list, int numVisibleEntry )
{
  int w = list->count() > 0 ? list->item(0)->height(list) :
    list->fontMetrics().lineSpacing();

  if( w < 0 ) { w = 10; }
  if( numVisibleEntry <= 0 ) { numVisibleEntry = 4; }
  return( w * numVisibleEntry + 2 * list->frameWidth() );
}

class FontChooser::KFontChooserPrivate
{
public:
    KFontChooserPrivate()
        { m_palette.setColor(QPalette::Active, QColorGroup::Text, Qt::black);
          m_palette.setColor(QPalette::Active, QColorGroup::Base, Qt::white); }
    QPalette m_palette;
};

FontChooser::FontChooser(QWidget *parent, const char *name,
			   bool onlyFixed, const QStringList &fontList,
			   bool makeFrame, int visibleListSize )
  : QWidget(parent, name), usingFixed(onlyFixed)
{
  d = new KFontChooserPrivate;
  QVBoxLayout *topLayout = new QVBoxLayout( this, 0, KDialog::spacingHint() );

  QWidget *page;
  QGridLayout *gridLayout;
  int row = 0;
  if( makeFrame == true )
  {
    page = new QGroupBox( i18n("Requested Font"), this );
    topLayout->addWidget(page);
    gridLayout = new QGridLayout( page, 5, 3, KDialog::spacingHint() );
    gridLayout->addRowSpacing( 0, fontMetrics().lineSpacing() );
    row = 1;
  }
  else
  {
    page = new QWidget( this );
    topLayout->addWidget(page);
    gridLayout = new QGridLayout( page, 4, 3, 0, KDialog::spacingHint() );
  }

  //
  // first, create the labels across the top
  //
  familyLabel = new QLabel( i18n("Font"), page, "familyLabel" );
  gridLayout->addWidget(familyLabel, row, 0, AlignLeft );
  styleLabel = new QLabel( i18n("Font style"), page, "styleLabel");
  gridLayout->addWidget(styleLabel, row, 1, AlignLeft);
  sizeLabel = new QLabel( i18n("Size"), page, "sizeLabel");
  gridLayout->addWidget(sizeLabel, row, 2, AlignLeft);

  row ++;

  //
  // now create the actual boxes that hold the info
  //
  familyListBox = new KListBox( page, "familyListBox");
  gridLayout->addWidget( familyListBox, row, 0 );
  connect(familyListBox, SIGNAL(highlighted(const QString &)),
	  SLOT(family_chosen_slot(const QString &)));
  if(fontList.count() != 0)
  {
    familyListBox->insertStringList(fontList);
  }
  else
  {
    fillFamilyListBox(onlyFixed);
  }

  familyListBox->setMinimumWidth( minimumListWidth( familyListBox ) );
  familyListBox->setMinimumHeight(
    minimumListHeight( familyListBox, visibleListSize  ) );

  styleListBox = new KListBox( page, "styleListBox");
  gridLayout->addWidget(styleListBox, row, 1);
  styleListBox->insertItem(i18n("Regular"));
  styleListBox->insertItem(i18n("Italic"));
  styleListBox->insertItem(i18n("Bold"));
  styleListBox->insertItem(i18n("Bold Italic"));
  styleListBox->setMinimumWidth( minimumListWidth( styleListBox ) );
  styleListBox->setMinimumHeight(
    minimumListHeight( styleListBox, visibleListSize  ) );

  connect(styleListBox, SIGNAL(highlighted(const QString &)),
	  SLOT(style_chosen_slot(const QString &)));


  sizeListBox = new KListBox( page, "sizeListBox");
  gridLayout->addWidget(sizeListBox, row, 2);

  const char *c[] =
  {
    "4",  "5",  "6",  "7",
    "8",  "9",  "10", "11",
    "12", "13", "14", "15",
    "16", "17", "18", "19",
    "20", "22", "24", "26",
    "28", "32", "48", "64",
    0
  };
  for(int i = 0; c[i] != 0; i++)
  {
    sizeListBox->insertItem(QString::fromLatin1(c[i]));
  }
  sizeListBox->setMinimumWidth( minimumListWidth(sizeListBox) +
    sizeListBox->fontMetrics().maxWidth() );
  sizeListBox->setMinimumHeight(
    minimumListHeight( sizeListBox, visibleListSize  ) );


  connect( sizeListBox, SIGNAL(highlighted(const QString&)),
	   SLOT(size_chosen_slot(const QString&)) );

  row ++;
  charsetLabel = new QLabel( page, "charsetLabel");
  charsetLabel->setText(i18n("Character set:"));
  gridLayout->addWidget(charsetLabel, 3, 0, AlignRight);
  charsetsCombo = new QComboBox(true, page, "charsetsCombo");
  gridLayout->addMultiCellWidget(charsetsCombo, 3, 3, 1, 2);
  charsetsCombo->setInsertionPolicy(QComboBox::NoInsertion);
  connect(charsetsCombo, SIGNAL(activated(const QString&)),
	  SLOT(charset_chosen_slot(const QString&)));

  row ++;
  sampleEdit = new QLineEdit( page, "sampleEdit");
  QFont tmpFont( KGlobalSettings::generalFont().family(), 64, QFont::Black );
  sampleEdit->setFont(tmpFont);
  sampleEdit->setText(i18n("The Quick Brown Fox Jumps Over The Lazy Dog"));
  sampleEdit->setMinimumHeight( sampleEdit->fontMetrics().lineSpacing() );
  sampleEdit->setAlignment(Qt::AlignCenter);
  gridLayout->addMultiCellWidget(sampleEdit, 4, 4, 0, 2);
  connect(this, SIGNAL(fontSelected(const QFont &)),
	  SLOT(displaySample(const QFont &)));

  QVBoxLayout *vbox;
  if( makeFrame == true )
  {
    page = new QGroupBox( i18n("Actual Font"), this );
    topLayout->addWidget(page);
    vbox = new QVBoxLayout( page, KDialog::spacingHint() );
    vbox->addSpacing( fontMetrics().lineSpacing() );
  }
  else
  {
    page = new QWidget( this );
    topLayout->addWidget(page);
    vbox = new QVBoxLayout( page, 0, KDialog::spacingHint() );
    QLabel *label = new QLabel( i18n("Actual Font"), page );
    vbox->addWidget( label );
  }

  xlfdEdit = new QLineEdit( page, "xlfdEdit" );
  vbox->addWidget( xlfdEdit );

  // lets initialize the display if possible
  setFont( KGlobalSettings::generalFont(), usingFixed );
  // Create displayable charsets list
  fillCharsetsCombo();

  KConfig *config = KGlobal::config();
  KConfigGroupSaver saver(config, QString::fromLatin1("General"));
  showXLFDArea(config->readBoolEntry(QString::fromLatin1("fontSelectorShowXLFD"), false));
}

FontChooser::~FontChooser()
{
  delete d;
}

void FontChooser::setColor( const QColor & col, QPalette::ColorGroup group, QColorGroup::ColorRole role )
{
  d->m_palette.setColor(group, role, col);
  QPalette pal = sampleEdit->palette();
  pal.setColor(group, role, col);
  sampleEdit->setPalette(pal);
}

QColor FontChooser::color( QPalette::ColorGroup group, QColorGroup::ColorRole role ) const
{
  return d->m_palette.color(group, role);
}

QSize FontChooser::sizeHint( void ) const
{
  return( minimumSizeHint() );
}


void FontChooser::enableColumn( int column, bool state )
{
  if( column & FamilyList )
  {
    familyLabel->setEnabled(state);
    familyListBox->setEnabled(state);
  }
  if( column & StyleList )
  {
    styleLabel->setEnabled(state);
    styleListBox->setEnabled(state);
  }
  if( column & SizeList )
  {
    sizeLabel->setEnabled(state);
    sizeListBox->setEnabled(state);
  }
  if( column & CharsetList )
  {
    charsetLabel->setEnabled(state);
    charsetsCombo->setEnabled(state);
  }
}


void FontChooser::setFont( const QFont& aFont, bool onlyFixed )
{
  selFont = aFont;
  if( onlyFixed != usingFixed)
  {
    usingFixed = onlyFixed;
    fillFamilyListBox(usingFixed);
  }
  setupDisplay();
  displaySample(selFont);
}

void FontChooser::setCharset( const QString & charset )
{
    for ( int i = 0; i < charsetsCombo->count(); i++ ) {
        if ( charsetsCombo->text( i ) == charset ) {
            charsetsCombo->setCurrentItem( i );
            return;
        }
    }
}

void FontChooser::charset_chosen_slot(const QString& chset)
{
  KCharsets *charsets = KGlobal::charsets();
  if (chset == i18n("default")) {
    charsets->setQFont(selFont, KGlobal::locale()->charset());
  } else {
    kdDebug() << "FontChooser::charset_chosen_slot chset=" << chset << endl;
    charsets->setQFont(selFont, chset);
  }

  emit fontSelected(selFont);
}

QString FontChooser::charset() const
{
  return charsetsCombo->currentText();
}

void FontChooser::fillCharsetsCombo()
{
  int i;
  KCharsets *charsets=KGlobal::charsets();

  charsetsCombo->clear();
  QStringList sets=charsets->availableCharsetNames(selFont.family());
  charsetsCombo->insertItem( i18n("default") );
  for ( QStringList::Iterator it = sets.begin(); it != sets.end(); ++it )
      charsetsCombo->insertItem( *it );
  // This doesn't make any sense, according to everyone I asked (DF)
  // charsetsCombo->insertItem( i18n("any") );

  QString charset=charsets->xCharsetName(selFont.charSet());
  for(i = 0; i < charsetsCombo->count(); i++){
    if (charset == charsetsCombo->text(i)){
      charsetsCombo->setCurrentItem(i);
      break;
    }
  }
}

void FontChooser::family_chosen_slot(const QString& family)
{
  selFont.setFamily(family);

  fillCharsetsCombo();

  emit fontSelected(selFont);
}

void FontChooser::size_chosen_slot(const QString& size){

  QString size_string = size;

  selFont.setPointSize(size_string.toInt());
  emit fontSelected(selFont);
}

void FontChooser::style_chosen_slot(const QString& style)
{
  QString style_string = style;

  if ( style_string.find(i18n("Italic")) != -1)
    selFont.setItalic(true);
  else
    selFont.setItalic(false);
  if ( style_string.find(i18n("Bold")) != -1)
    selFont.setBold(true);
  else
    selFont.setBold(false);
  emit fontSelected(selFont);
}

void FontChooser::displaySample(const QFont& font)
{
  sampleEdit->setFont(font);
  sampleEdit->setCursorPosition(0);
  xlfdEdit->setText(font.rawName());
  xlfdEdit->setCursorPosition(0);
}

void FontChooser::setupDisplay()
{
  QString aString;
  int numEntries, i=0;
  //  bool found;

  numEntries =  familyListBox->count();
  aString = selFont.family();
  //  found = false;

  for (i = 0; i < numEntries; i++) {
    if (aString.lower() == (familyListBox->text(i).lower())) {
      familyListBox->setCurrentItem(i);
      //      found = true;
      break;
    }
  }


  numEntries =  sizeListBox->count();
  aString.setNum(selFont.pointSize());
  //  found = false;

  for (i = 0; i < numEntries; i++){
    if (aString == sizeListBox->text(i)) {
      sizeListBox->setCurrentItem(i);
      //      found = true;
      break;
    }
  }

  i = (selFont.bold() ? 2 : 0);
  i += (selFont.italic() ? 1 : 0);

  styleListBox->setCurrentItem(i);

  // Re-create displayable charsets list
  fillCharsetsCombo();
}


void FontChooser::getFontList( QStringList &list, bool fixed )
{
  QFontDatabase dbase;
  QStringList lstSys(dbase.families( false ));

  // Since QFontDatabase doesn't have any easy way of returning just
  // the fixed width fonts, we'll do it in a very hacky way
  if (fixed)
  {
    QStringList lstFixed;
    for (QStringList::Iterator it = lstSys.begin(); it != lstSys.end(); ++it)
    {
        // To get the fixed with info (known as fixed pitch in Qt), we
        // need to get a QFont or QFontInfo object.  To do this, we
        // need a family name, style, and point size.
        QStringList styles(dbase.styles(*it));
        QStringList::Iterator astyle = styles.begin();

        QFontInfo info(dbase.font(*it, *astyle, 10));
        if (info.fixedPitch())
          lstFixed.append(*it);
    }

    // Fallback.. if there are no fixed fonts found, it's probably a
    // bug in the font server or Qt.  In this case, just use 'fixed'
    if (lstFixed.count() == 0)
      lstFixed.append("fixed");

    lstSys = lstFixed;
  }

  lstSys.sort();

  list = lstSys;
}


void FontChooser::getFontList( QStringList &list, const char *pattern )
{
  int num;
  char **xFonts = XListFonts( qt_xdisplay(), pattern, 2000, &num );

  for( int i = 0; i < num; i++ )
  {
    addFont( list, xFonts[i] );
  }
  XFreeFontNames( xFonts );
}



void FontChooser::addFont( QStringList &list, const char *xfont )
{
  const char *ptr = strchr( xfont, '-' );
  if ( !ptr )
    return;

  ptr = strchr( ptr + 1, '-' );
  if ( !ptr )
    return;

  QString font = QString::fromLatin1(ptr + 1);

  int pos;
  if ( ( pos = font.find( '-' ) ) > 0 ) {
    font.truncate( pos );

    if ( font.find( QString::fromLatin1("open look"), 0, false ) >= 0 )
      return;

    QStringList::Iterator it = list.begin();

    for ( ; it != list.end(); ++it )
      if ( *it == font )
	return;
    list.append( font );
  }
}

void FontChooser::fillFamilyListBox(bool onlyFixedFonts)
{
  QStringList fontList;
  getFontList(fontList, onlyFixedFonts);
  familyListBox->clear();
  familyListBox->insertStringList(fontList);
}

void FontChooser::showXLFDArea(bool show)
{
  if( show == true )
  {
    xlfdEdit->parentWidget()->show();
  }
  else
  {
    xlfdEdit->parentWidget()->hide();
  }
}

