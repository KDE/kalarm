/*  -*- c++ -*-
    kmime_codecs.h

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

#ifndef __KMIME_CODECS__
#define __KMIME_CODECS__

#include <qasciidict.h>
#if defined(QT_THREAD_SUPPORT)
#  include <qmutex.h>
#endif

#include <qcstring.h> // QByteArray

#include <kdebug.h> // for kdFatal()

namespace KMime {

// forward declarations:
class Encoder;
class Decoder;

/** Abstract base class of codecs like base64 and
    quoted-printable. It's a singleton.
    
    @short Codecs for common mail transfer encodings.
    @author Marc Mutz <mutz@kde.org>
*/
class Codec {
protected:

  static QAsciiDict<Codec> all;
#if defined(QT_THREAD_SUPPORT)
  static QMutex dictLock;
#endif

  Codec() {}
private:
  static void fillDictionary();
  
public:
  static Codec * codecForName( const char * name );
  static Codec * codecForName( const QCString & name );

  virtual int maxEncodedSizeFor( int insize, bool withCRLF=false ) const = 0;
  virtual int maxDecodedSizeFor( int insize, bool withCRLF=false ) const = 0;
  
  virtual Encoder * makeEncoder( bool withCRLF=false ) const = 0;
  virtual Decoder * makeDecoder( bool withCRLF=false ) const = 0;

  /**
   * Convenience wrapper that can be used for small chunks of data
   * when you can provide a large enough buffer. The default
   * implementation creates an @ref Encoder and uses it.
   *
   * Encodes a chunk of bytes starting at @p scursor and extending to
   * @p send into the buffer described by @p dcursor and @p dend.
   *
   * This function doesn't support chaining of blocks. The returned
   * block cannot be added to, but you don't need to finalize it, too.
   *
   * Example usage (@p in contains the input data):
   * <pre>
   * KMime::Codec * codec = KMime::Codec::codecForName( "base64" );
   * kdFatal( !codec ) << "no base64 codec found!?" << endl;
   * QByteArray out( in.size()*1.4 ); // crude maximal size of b64 encoding
   * QByteArray::Iterator iit = in.begin();
   * QButeArray::Iterator oit = out.begin();
   * if ( !codec->encode( iit, in.end(), oit, out.end() ) ) {
   *   kdDebug() << "output buffer too small" << endl;
   *   return;
   * }
   * kdDebug() << "Size of encoded data: " << oit - out.begin() << endl;
   * </pre>
   *
   * @param scursor/send begin and end of input buffer
   * @param dcursor/dend begin and end of output buffer
   * @param withCRLF If true, make the lineends CRLF, else make them LF only.
   *
   * @return false if the encoded data didn't fit into the output
   * buffer.
   **/
  virtual bool encode( const char* & scursor, const char * const send,
		       char* & dcursor, const char * const dend,
		       bool withCRLF=false ) const;
  
  /**
   * Convenience wrapper that can be used for small chunks of data
   * when you can provide a large enough buffer. The default
   * implementation creates a @ref Decoder and uses it.
   *
   * Decodes a chunk of bytes starting at @p scursor and extending to
   * @p send into the buffer described by @p dcursor and @p dend.
   *
   * This function doesn't support chaining of blocks. The returned
   * block cannot be added to, but you don't need to finalize it, too.
   *
   * Example usage (@p in contains the input data):
   * <pre>
   * KMime::Codec * codec = KMime::Codec::codecForName( "base64" );
   * kdFatal( !codec ) << "no base64 codec found!?" << endl;
   * QByteArray out( in.size() ); // good guess for any encoding...
   * QByteArray::Iterator iit = in.begin();
   * QButeArray::Iterator oit = out.begin();
   * if ( !codec->decode( iit, in.end(), oit, out.end() ) ) {
   *   kdDebug() << "output buffer too small" << endl;
   *   return;
   * }
   * kdDebug() << "Size of decoded data: " << oit - out.begin() << endl;
   * </pre>
   *
   * @param scursor/send begin and end of input buffer
   * @param dcursor/dend begin and end of output buffer
   * @param withCRLF If true, make the lineends CRLF, else make them LF only.
   *
   * @return false if the decoded data didn't fit into the output
   * buffer.
   **/
  virtual bool decode( const char* & scursor, const char * const send,
		       char* & dcursor, const char * const dend,
		       bool withCRLF=false ) const;

  /**
   * Even more convenient, but also a bit slower and more memory
   * intensive, since it allocates storage for the worst case and then
   * shrinks the result QByteArray to the actual size again.
   *
   * For use with small @p src.
   **/
  virtual QByteArray encode( const QByteArray & src, bool withCRLF=false ) const;

  /**
   * Even more convenient, but also a bit slower and more memory
   * intensive, since it allocates storage for the worst case and then
   * shrinks the result QByteArray to the actual size again.
   *
   * For use with small @p src.
   **/
  virtual QByteArray decode( const QByteArray & src, bool withCRLF=false ) const;

  /**
   * @return the name of the encoding. Guaranteed to be lowercase.
   */
  virtual const char * name() const = 0;
  
  virtual ~Codec() {}
  
};
  
/**
 * Stateful decoder class, modelled after @ref QTextDecoder.
 *
 * @sect Overview
 *
 * KMime decoders are designed to be able to process encoded data in
 * chunks of arbitrary size and to work with output buffers of also
 * arbitrary size. They maintain any state necessary to go on where
 * the previous call left off.
 *
 * The class consists of only two methods of interest: @ref decode,
 * which decodes an input block and @ref finalize, which flushes any
 * remaining data to the output stream.
 *
 * Typically, you will create a decoder instance, call @ref decode as
 * often as necessary, then call @ref finalize (most often a single
 * call suffices, but it might be that during that call the output
 * buffer is filled, so you should be prepared to call @ref finalize
 * as often as necessary, ie. until it returns @p true).
 *
 * @sect Return Values
 *
 * Both methods return @p true to indicate that they've finished their
 * job. For @ref decode, a return value of @p true means that the
 * current input block has been finished (@p false most often means
 * that the output buffer is full, but that isn't required
 * behaviour. The @ref decode call is free to return at arbitrary
 * times during processing).
 *
 * For @ref finalize, a return value of @p true means that all data
 * implicitly or explicitly stored in the decoder instance has been
 * flushed to the output buffer. A @p false return value should be
 * interpreted as "check if the output buffer is full and call me
 * again", just as with @ref decode.
 *
 * @sect Usage Pattern
 *
 * Since the decoder maintains state, you can only use it once. After
 * a sequence of input blocks has been processed, you @ref finalize
 * the output and then delete the decoder instance. If you want to
 * process another input block sequence, you create a new instance.
 *
 * Typical usage (@p in contains the (base64-encoded) input data),
 * taking into account all the conventions detailed above:
 *
 * <pre>
 * KMime::Codec * codec = KMime::Codec::codecForName( "base64" );
 * kdFatal( !codec ) << "No codec found for base64!" << endl;
 * KMime::Decoder * dec = codec->makeDecoder();
 * assert( dec ); // should not happen
 * QByteArray out( 256 ); // small buffer is enough ;-)
 * QByteArray::Iterator iit = in.begin();
 * QByteArray::Iterator oit = out.begin();
 * // decode the chunk
 * while ( !dec->decode( iit, in.end(), oit, out.end() ) )
 *   if ( oit == out.end() ) { // output buffer full, process contents
 *     do_something_with( out );
 *     oit = out.begin();
 *   }
 * // repeat while loop for each input block
 * // ...
 * // finish (flush remaining data from decoder):
 * while ( !dec->finish( oit, out.end() ) )
 *   if ( oit == out.end() ) { // output buffer full, process contents
 *     do_something_with( out );
 *     oit = out.begin();
 *   }
 * // now process last chunk:
 * out.resize( oit - out.begin() );
 * do_something_with( out );
 * // _delete_ the decoder, but not the codec:
 * delete dec;
 * </pre>
 *
 * @short Stateful CTE decoder class
 * @author Marc Mutz <mutz@kde.org>
 **/
class Decoder {
protected:
  friend class Codec;
  /**
   * Protected constructor. Use @ref KMime::Codec::makeDecoder to
   * create an instance. The bool parameter determines whether lines
   * end with CRLF (true) or LF (false, default).
   **/
  Decoder( bool withCRLF=false )
    : mWithCRLF( withCRLF ) {}
public:
  virtual ~Decoder() {}

  /** Decode a chunk of data, maintaining state information between
   *  calls. See class decumentation for calling conventions.
   **/
  virtual bool decode( const char* & scursor, const char * const send,
		       char* & dcursor, const char * const dend ) = 0;
  /** Call this method to finalize the output stream. Writes all
   *  remaining data and resets the decoder. See @ref KMime::Codec for
   *  calling conventions.
   **/
  virtual bool finish( char* & dcursor, const char * const dend ) = 0;

protected:
  const bool mWithCRLF;
};
  
/** Stateful encoder class, modelled after @ref QTextEncoder.
    @short Stateful encoder class
    @author Marc Mutz <mutz@kde.org>
*/
class Encoder {
protected:
  friend class Codec;
  /** Protected constructor. Use @ref KMime::Codec::makeEncoder if you
      want one. The bool parameter determines whether lines end with
      CRLF (true) or LF (false, default). */
  Encoder( bool withCRLF=false )
    : mOutputBufferCursor( 0 ), mWithCRLF( withCRLF ) {}
public:
  virtual ~Encoder() {}

  /** Encode a chunk of data, maintaining state information between
      calls. See @ref KMime::Codec for calling conventions. */
  virtual bool encode( const char* & scursor, const char * const send,
		       char* & dcursor, const char * const dend ) = 0;

  /** Call this method to finalize the output stream. Writes all
      remaining data and resets the encoder. See @ref KMime::Codec for
      calling conventions. */
  virtual bool finish( char* & dcursor, const char * const dend ) = 0;

protected:
  /** Space in the output buffer */
  enum { maxBufferedChars = 8 };

  /** Writes @p ch to the output stream or the output buffer,
      depending on whether or not the output stream has space left.
      @return true if written to the output stream, false if buffered. */
  bool write( char ch, char* & dcursor, const char * const dend ) {
    if ( dcursor != dend ) {
      // if there's space in the output stream, write there:
      *dcursor++ = ch;
      return true;
    } else {
      // else buffer the output:
      kdFatal( mOutputBufferCursor >= maxBufferedChars )
	<< "KMime::Encoder: internal buffer overflow!" << endl;
      mOutputBuffer[ mOutputBufferCursor++ ] = ch;
      return false;
    }
  }

  /** Writes characters from the output buffer to the output stream.
      Implementations of @ref encode and @ref finish should call this
      at the very beginning and for each iteration of the while loop.
      @return true if all chars could be written, false otherwise */
  bool flushOutputBuffer( char* & dcursor, const char * const dend );

  /** Convenience function. Outputs LF or CRLF, based on the state of
      @ref mWithCRLF */
  bool writeCRLF( char* & dcursor, const char * const dend ) {
    if ( mWithCRLF )
      write( '\r', dcursor, dend );
    return write( '\n', dcursor, dend );
  }

private:
  /** An output buffer to simplyfy some codecs. Use with @ref write
      and flushOutputBuffer */
  char mOutputBuffer[ maxBufferedChars ];
protected:
  uchar mOutputBufferCursor;
  const bool mWithCRLF;
};

}; // namespace KMime

#endif // __KMIME_CODECS__
