/*
    kmime_codec_uuencode.cpp

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2002 Marc Mutz <mutz@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2.0, as published by the Free Software Foundation.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, US
*/

#include "kmime_codec_uuencode.h"

#include <kdebug.h>

#include <cassert>

using namespace KMime;

namespace KMime {


class UUDecoder : public Decoder {
  uint mStepNo;
  uchar mAnnouncedOctetCount; // (on current line)
  uchar mCurrentOctetCount; // (on current line)
  uchar mOutbits;
  bool mLastWasCRLF : 1;
  bool mSawBegin : 1;      // whether we already saw ^begin...
  uint mIntoBeginLine : 3; // count #chars we compared against "begin" 0..5
  bool mSawEnd : 1;        // whether we already saw ^end...
  uint mIntoEndLine : 2;   // count #chars we compared against "end" 0..3

  void searchForBegin( const char* & scursor, const char * const send );

protected:
  friend class UUCodec;
  UUDecoder( bool withCRLF=false )
    : Decoder( withCRLF ), mStepNo(0),
      mAnnouncedOctetCount(0), mCurrentOctetCount(0),
      mOutbits(0), mLastWasCRLF(true),
      mSawBegin(false), mIntoBeginLine(0),
      mSawEnd(false), mIntoEndLine(0) {}

public:
  virtual ~UUDecoder() {}

  bool decode( const char* & scursor, const char * const send,
	       char* & dcursor, const char * const dend );
  // ### really needs no finishing???
  bool finish( char* & /*dcursor*/, const char * const /*dend*/ ) { return true; }
};



Encoder * UUCodec::makeEncoder( bool ) const {
  return 0; // encoding not supported
}

Decoder * UUCodec::makeDecoder( bool withCRLF ) const {
  return new UUDecoder( withCRLF );
}


  /********************************************************/
  /********************************************************/
  /********************************************************/



void UUDecoder::searchForBegin( const char* & scursor, const char * const send ) {
  static const char * begin = "begin\n";
  static const uint beginLength = 5; // sic!

  assert( !mSawBegin || mIntoBeginLine > 0 );

  while ( scursor != send ) {
    uchar ch = *scursor++;
    if ( ch == begin[mIntoBeginLine] ) {
      if ( mIntoBeginLine < beginLength ) {
	// found another char
	++mIntoBeginLine;
	if ( mIntoBeginLine == beginLength )
	  mSawBegin = true; // "begin" complete, now search the next \n...
      } else /* mIntoBeginLine == beginLength */ {
	// found '\n': begin line complete
	mLastWasCRLF = true;
	mIntoBeginLine = 0;
	return;
      }
    } else if ( mSawBegin ) {
      // OK, skip stuff until the next \n
    } else {
      kdWarning() << "UUDecoder: garbage before \"begin\", resetting parser"
		  << endl;
      mIntoBeginLine = 0;
    }
  }

}


// uuencoding just shifts all 6-bit octets by 32 (SP/' '), except NUL,
// which gets mapped to 0x60
static inline uchar uuDecode( uchar c ) {
  return ( c - ' ' ) // undo shift and
         & 0x3F;     // map 0x40 (0x60-' ') to 0...
}


bool UUDecoder::decode( const char* & scursor, const char * const send,
			char* & dcursor, const char * const dend )
{
  // First, check whether we still need to find the "begin" line:
  if ( !mSawBegin || mIntoBeginLine != 0 )
    searchForBegin( scursor, send );
  // or if we are past the end line:
  else if ( mSawEnd ) {
    scursor = send; // do nothing anymore...
    return true;
  }

  while ( dcursor != dend && scursor != send ) {
    uchar ch = *scursor++;
    uchar value;

    // Check whether we need to look for the "end" line:
    if ( mIntoEndLine > 0 ) {
      static const char * end = "end";
      static const uint endLength = 3;

      if ( ch == end[mIntoEndLine] ) {
	++mIntoEndLine;
	if ( mIntoEndLine == endLength ) {
	  mSawEnd = true;
	  scursor = send; // shortcut to the end
	  return true;
	}
	continue;
      } else {
	kdWarning() << "UUDecoder: invalid line octet count looks like \"end\" (mIntoEndLine = " << mIntoEndLine << " )!" << endl;
	mIntoEndLine = 0;
	// fall through...
      }
    }

    // Normal parsing:

    // The first char of a line is an encoding of the length of the
    // current line. We simply ignore it:
    if ( mLastWasCRLF ) {
      // reset char-per-line counter:
      mLastWasCRLF = false;
      mCurrentOctetCount = 0;

      // try to decode the chars-on-this-line announcement:
      if ( ch == 'e' ) // maybe the beginning of the "end"? ;-)
	mIntoEndLine = 1;
      else if ( ch > 0x60 )
	{} // ### invalid line length char: what shall we do??
      else if ( ch > ' ' )
	mAnnouncedOctetCount = uuDecode( ch );
      else if ( ch == '\n' )
	mLastWasCRLF = true; // oops, empty line

      continue;
    }

    // try converting ch to a 6-bit value:
    if ( ch > 0x60 )
      continue; // invalid char
    else if ( ch > ' ' )
      value = uuDecode( ch );
    else if ( ch == '\n' ) { // line end
      mLastWasCRLF = true;
      continue;
    } else
      continue;

    // add the new bits to the output stream and flush full octets:    
    switch ( mStepNo ) {
    case 0:
      mOutbits = value << 2;
      break;
    case 1:
      if ( mCurrentOctetCount < mAnnouncedOctetCount )
	*dcursor++ = (char)(mOutbits | value >> 4);
      ++mCurrentOctetCount;
      mOutbits = value << 4;
      break;
    case 2:
      if ( mCurrentOctetCount < mAnnouncedOctetCount )
	*dcursor++ = (char)(mOutbits | value >> 2);
      ++mCurrentOctetCount;
      mOutbits = value << 6;
      break;
    case 3:
      if ( mCurrentOctetCount < mAnnouncedOctetCount )
	*dcursor++ = (char)(mOutbits | value);
      ++mCurrentOctetCount;
      mOutbits = 0;
      break;
    default:
      assert( 0 );
    }
    mStepNo = (mStepNo + 1) % 4;

    // check whether we ran over the announced octet count for this line:
    kdWarning( mCurrentOctetCount == mAnnouncedOctetCount + 1 ) 
      << "UUDecoder: mismatch between announced ("
      << mAnnouncedOctetCount << ") and actual line octet count!" << endl;

  }

  // return false when caller should call us again:
  return (scursor == send);
} // UUDecoder::decode()


}; // namespace KMime
