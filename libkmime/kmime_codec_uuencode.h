/*
    kmime_codec_uuencode.h

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2002 Marc Mutz <mutz@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2.0, as published by the Free Software Foundation.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, US
*/

#ifndef __KMIME_CODEC_UUENCODE_H__
#define __KMIME_CODEC_UUENCODE_H__

#include "kmime_codecs.h"

namespace KMime {

class UUCodec : public Codec {
protected:
  friend class Codec;
  UUCodec() : Codec() {}

public:
  virtual ~UUCodec() {}

  const char * name() const {
    return "x-uuencode";
  }

  int maxEncodedSizeFor( int insize, bool withCRLF=false ) const {
    (void)withCRLF;
    return insize; // we have no encoder!
  }

  int maxDecodedSizeFor( int insize, bool withCRLF=false ) const {
    // assuming all characters are part of the uuencode stream (which
    // does almost never hold due to required linebreaking; but
    // additonal non-uu chars don't affect the output size), each
    // 4-tupel of them becomes a 3-tupel in the decoded octet
    // stream. So:
    int result = ( ( insize + 3 ) / 4 ) * 3;
    // but all of them may be \n, so
    if ( withCRLF )
      result *= 2; // :-o

    return result;
  }

  Encoder * makeEncoder( bool withCRLF=false ) const;
  Decoder * makeDecoder( bool withCRLF=false ) const;
};

}; // namespace KMime

#endif // __KMIME_CODEC_UUENCODE_H__
