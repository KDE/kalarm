/*  -*- c++ -*-
    kmime_codec_qp.h

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2001-2002 the KMime authors.
    See file AUTHORS for details

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License,
    version 2.0, as published by the Free Software Foundation.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, US
*/

#ifndef __KMIME_CODEC_QP__
#define __KMIME_CODEC_QP__

#include "kmime_codecs.h"

namespace KMime {


class QuotedPrintableCodec : public Codec {
protected:
  friend class Codec;
  QuotedPrintableCodec() : Codec() {}

public:
  virtual ~QuotedPrintableCodec() {}

  const char * name() const {
    return "quoted-printable";
  }

  int maxEncodedSizeFor( int insize, bool withCRLF=false ) const {
    // all chars encoded:
    int result = 3*insize;
    // then after 25 hexchars comes a soft linebreak: =(\r)\n
    result += (withCRLF ? 3 : 2) * (insize/25);

    return result;
  }

  int maxDecodedSizeFor( int insize, bool withCRLF=false ) const {
    // all chars unencoded:
    int result = insize;
    // but maybe all of them are \n and we need to make them \r\n :-o
    if ( withCRLF )
      result += insize;

    return result;
  }

  Encoder * makeEncoder( bool withCRLF=false ) const;
  Decoder * makeDecoder( bool withCRLF=false ) const;
};


class Rfc2047QEncodingCodec : public Codec {
protected:
  friend class Codec;
  Rfc2047QEncodingCodec() : Codec() {}

public:
  virtual ~Rfc2047QEncodingCodec() {}

  const char * name() const {
    return "q";
  }

  int maxEncodedSizeFor( int insize, bool withCRLF=false ) const {
    (void)withCRLF; // keep compiler happy
    // this one is simple: We don't do linebreaking, so all that can
    // happen is that every char needs encoding, so:
    return 3*insize;
  }

  int maxDecodedSizeFor( int insize, bool withCRLF=false ) const {
    (void)withCRLF; // keep compiler happy
    // equally simple: nothing is encoded at all, so:
    return insize;
  }

  Encoder * makeEncoder( bool withCRLF=false ) const;
  Decoder * makeDecoder( bool withCRLF=false ) const;
};


class Rfc2231EncodingCodec : public Codec {
protected:
  friend class Codec;
  Rfc2231EncodingCodec() : Codec() {}

public:
  virtual ~Rfc2231EncodingCodec() {}

  const char * name() const {
    return "x-kmime-rfc2231";
  }

  int maxEncodedSizeFor( int insize, bool withCRLF=false ) const {
    (void)withCRLF; // keep compiler happy
    // same as for "q" encoding:
    return 3*insize;
  }

  int maxDecodedSizeFor( int insize, bool withCRLF=false ) const {
    (void)withCRLF; // keep compiler happy
    // same as for "q" encoding:
    return insize;
  }

  Encoder * makeEncoder( bool withCRLF=false ) const;
  Decoder * makeDecoder( bool withCRLF=false ) const;
};


}; // namespace KMime

#endif // __KMIME_CODEC_QP__
