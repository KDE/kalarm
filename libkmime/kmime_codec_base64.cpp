/*
    kmime_codec_base64.cpp

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2001 the KMime authors.
    See file AUTHORS for details

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2.0, as published by the Free Software Foundation.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, US
*/

#include "kmime_codec_base64.h"

#include <kdebug.h>

#include <cassert>

using namespace KMime;

namespace KMime {

// codec for base64 as specified in RFC 2045
  //class Base64Codec;
  //class Base64Decoder;
  //class Base64Encoder;

// codec for the B encoding as specified in RFC 2047
  //class Rfc2047BEncodingCodec;
  //class Rfc2047BEncodingEncoder;
  //class Rfc2047BEncodingDecoder;


static const uchar base64DecodeMap[128] = {
  64, 64, 64, 64, 64, 64, 64, 64,  64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64,  64, 64, 64, 64, 64, 64, 64, 64,
  
  64, 64, 64, 64, 64, 64, 64, 64,  64, 64, 64, 62, 64, 64, 64, 63,
  52, 53, 54, 55, 56, 57, 58, 59,  60, 61, 64, 64, 64, 64, 64, 64,
  
  64,  0,  1,  2,  3,  4,  5,  6,   7,  8,  9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22,  23, 24, 25, 64, 64, 64, 64, 64,
  
  64, 26, 27, 28, 29, 30, 31, 32,  33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48,  49, 50, 51, 64, 64, 64, 64, 64
};

static const char base64EncodeMap[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};


class Base64Decoder : public Decoder {
  uint mStepNo;
  uchar mOutbits;
  bool mSawPadding : 1;

protected:
  friend class Base64Codec;
  Base64Decoder( bool withCRLF=false )
    : Decoder( withCRLF ), mStepNo(0), mOutbits(0),
      mSawPadding(false) {}

public:
  virtual ~Base64Decoder() {}

  bool decode( const char* & scursor, const char * const send,
	       char* & dcursor, const char * const dend );
  // ### really needs no finishing???
  bool finish( char* & /*dcursor*/, const char * const /*dend*/ ) { return true; }
};



class Base64Encoder : public Encoder {
  uint mStepNo;
  /** number of already written base64-quartets on current line */
  uint mWrittenPacketsOnThisLine;
  uchar mNextbits;
  bool mInsideFinishing : 1;

protected:
  friend class Rfc2047BEncodingCodec;
  friend class Rfc2047BEncodingEncoder;
  friend class Base64Codec;
  Base64Encoder( bool withCRLF=false )
    : Encoder( withCRLF ), mStepNo(0), mWrittenPacketsOnThisLine(0),
      mNextbits(0), mInsideFinishing(false) {}

  bool generic_finish( char* & dcursor, const char * const dend,
		       bool withLFatEnd );

public:
  virtual ~Base64Encoder() {}

  bool encode( const char* & scursor, const char * const send,
	       char* & dcursor, const char * const dend );

  bool finish( char* & dcursor, const char * const dend );

protected:
  bool writeBase64( uchar ch, char* & dcursor, const char * const dend ) {
    return write( base64EncodeMap[ ch ], dcursor, dend );
  }
};



class Rfc2047BEncodingEncoder : public Base64Encoder {
protected:
  friend class Rfc2047BEncodingCodec;
  Rfc2047BEncodingEncoder( bool withCRLF=false )
    : Base64Encoder( withCRLF ) {};
public:
  bool encode( const char* & scursor, const char * const send,
	       char* & dcursor, const char * const dend );
  bool finish( char* & dcursor, const char * const dend );
};


Encoder * Base64Codec::makeEncoder( bool withCRLF ) const {
  return new Base64Encoder( withCRLF );
}

Decoder * Base64Codec::makeDecoder( bool withCRLF ) const {
  return new Base64Decoder( withCRLF );
}

Encoder * Rfc2047BEncodingCodec::makeEncoder( bool withCRLF ) const {
  return new Rfc2047BEncodingEncoder( withCRLF );
}

  /********************************************************/
  /********************************************************/
  /********************************************************/


bool Base64Decoder::decode( const char* & scursor, const char * const send,
			    char* & dcursor, const char * const dend )
{
  while ( dcursor != dend && scursor != send ) {
    uchar ch = *scursor++;
    uchar value;

    // try converting ch to a 6-bit value:
    if ( ch < 128 )
      value = base64DecodeMap[ ch ];
    else
      value = 64;

    // ch isn't of the base64 alphabet, check for other significant chars:
    if ( value >= 64 ) {
      if ( ch == '=' ) {
	// padding:
	if ( mStepNo == 0 || mStepNo == 1) {
	  if (!mSawPadding) {
	    // malformed
	    kdWarning() << "Base64Decoder: unexpected padding "
	      "character in input stream" << endl;
	  }
	  mSawPadding = true;
	  break;
	} else if ( mStepNo == 2 ) {
	  // ok, there should be another one
	} else if ( mStepNo == 3 ) {
	  // ok, end of encoded stream
	  mSawPadding = true;
	  break;
	}
	mSawPadding = true;
	mStepNo = (mStepNo + 1) % 4;
	continue;
      } else {
	// non-base64 alphabet
	continue;
      }
    }

    if ( mSawPadding ) {
      kdWarning() << "Base64Decoder: Embedded padding character "
	"encountered!" << endl;
      return true;
    }

    // add the new bits to the output stream and flush full octets:    
    switch ( mStepNo ) {
    case 0:
      mOutbits = value << 2;
      break;
    case 1:
      *dcursor++ = (char)(mOutbits | value >> 4);
      mOutbits = value << 4;
      break;
    case 2:
      *dcursor++ = (char)(mOutbits | value >> 2);
      mOutbits = value << 6;
      break;
    case 3:
      *dcursor++ = (char)(mOutbits | value);
      mOutbits = 0;
      break;
    default:
      assert( 0 );
    }
    mStepNo = (mStepNo + 1) % 4;
  }

  // return false when caller should call us again:
  return (scursor == send);
} // Base64Decoder::decode()



bool Base64Encoder::encode( const char* & scursor, const char * const send,
			    char* & dcursor, const char * const dend ) {
  const uint maxPacketsPerLine = 76 / 4;

  // detect when the caller doesn't adhere to our rules:
  if ( mInsideFinishing ) return true;

  while ( scursor != send && dcursor != dend ) {
    // properly empty the output buffer before starting something new:
    // ### fixme: we can optimize this away, since the buffer isn't
    // written to anyway (most of the time)
    if ( mOutputBufferCursor && !flushOutputBuffer( dcursor, dend ) )
      return (scursor == send);

    uchar ch = *scursor++;
    // mNextbits   // (part of) value of next sextet

    // check for line length;
    if ( mStepNo == 0 && mWrittenPacketsOnThisLine >= maxPacketsPerLine ) {
      writeCRLF( dcursor, dend );
      mWrittenPacketsOnThisLine = 0;
    }

    // depending on mStepNo, extract value and mNextbits from the
    // octet stream:
    switch ( mStepNo ) {
    case 0:
      assert( mNextbits == 0 );
      writeBase64( ch >> 2, dcursor, dend ); // top-most 6 bits -> output
      mNextbits = (ch & 0x3) << 4; // 0..1 bits -> 4..5 in mNextbits
      break;
    case 1:
      assert( (mNextbits & ~0x30) == 0 );
      writeBase64( mNextbits | ch >> 4, dcursor, dend ); // 4..7 bits -> 0..3 in value
      mNextbits = (ch & 0xf) << 2; // 0..3 bits -> 2..5 in mNextbits
      break;
    case 2:
      assert( (mNextbits & ~0x3C) == 0 );
      writeBase64( mNextbits | ch >> 6, dcursor, dend ); // 6..7 bits -> 0..1 in value
      writeBase64( ch & 0x3F, dcursor, dend ); // 0..5 bits -> output
      mNextbits = 0;
      mWrittenPacketsOnThisLine++;
      break;
    default:
      assert( 0 );
    }
    mStepNo = ( mStepNo + 1 ) % 3;
  }

  if ( mOutputBufferCursor ) flushOutputBuffer( dcursor, dend );

  return (scursor == send);
}


bool Rfc2047BEncodingEncoder::encode( const char* & scursor,
				      const char * const send,
				      char* & dcursor,
				      const char * const dend )
{
  // detect when the caller doesn't adhere to our rules:
  if ( mInsideFinishing ) return true;

  while ( scursor != send && dcursor != dend ) {
    // properly empty the output buffer before starting something new:
    // ### fixme: we can optimize this away, since the buffer isn't
    // written to anyway (most of the time)
    if ( mOutputBufferCursor && !flushOutputBuffer( dcursor, dend ) )
      return (scursor == send);

    uchar ch = *scursor++;
    // mNextbits   // (part of) value of next sextet

    // depending on mStepNo, extract value and mNextbits from the
    // octet stream:
    switch ( mStepNo ) {
    case 0:
      assert( mNextbits == 0 );
      writeBase64( ch >> 2, dcursor, dend ); // top-most 6 bits -> output
      mNextbits = (ch & 0x3) << 4; // 0..1 bits -> 4..5 in mNextbits
      break;
    case 1:
      assert( (mNextbits & ~0x30) == 0 );
      writeBase64( mNextbits | ch >> 4, dcursor, dend ); // 4..7 bits -> 0..3 in value
      mNextbits = (ch & 0xf) << 2; // 0..3 bits -> 2..5 in mNextbits
      break;
    case 2:
      assert( (mNextbits & ~0x3C) == 0 );
      writeBase64( mNextbits | ch >> 6, dcursor, dend ); // 6..7 bits -> 0..1 in value
      writeBase64( ch & 0x3F, dcursor, dend ); // 0..5 bits -> output
      mNextbits = 0;
      break;
    default:
      assert( 0 );
    }
    mStepNo = ( mStepNo + 1 ) % 3;
  }

  if ( mOutputBufferCursor ) flushOutputBuffer( dcursor, dend );

  return (scursor == send);
}


bool Base64Encoder::finish( char* & dcursor, const char * const dend ) {
  return generic_finish( dcursor, dend, true );
}

bool Rfc2047BEncodingEncoder::finish( char* & dcursor,
				      const char * const dend ) {
  return generic_finish( dcursor, dend, false );
}

bool Base64Encoder::generic_finish( char* & dcursor, const char * const dend,
				    bool withLFatEnd )
{
  if ( mInsideFinishing )
    return flushOutputBuffer( dcursor, dend );

  if ( mOutputBufferCursor && !flushOutputBuffer( dcursor, dend ) )
    return false;

  mInsideFinishing = true;

  //
  // writing out the last mNextbits...
  //
  switch ( mStepNo ) {
  case 1: // 2 mNextbits waiting to be written. Needs two padding chars:
  case 2: // 4 or 6 mNextbits waiting to be written. Completes a block
    writeBase64( mNextbits, dcursor, dend );
    mNextbits = 0;
    break;
  case 0: // no padding, nothing to be written, except possibly the CRLF
    assert( mNextbits == 0 );
    break;
  default:
    assert( 0 );
  }

  //
  // adding padding...
  //
  switch( mStepNo ) {
  case 1:
    write( '=', dcursor, dend );
    // fall through:
  case 2:
    write( '=', dcursor, dend );
    // fall through:
  case 0: // completed an quartet - add CRLF
    if ( withLFatEnd )
      writeCRLF( dcursor, dend );
    return flushOutputBuffer( dcursor, dend );
  default:
    assert( 0 );
  }
}






}; // namespace KMime
