/*
 * This is a cut-down version of libkdenetwork/kmime_header_parsing.cpp,
 * with a few amendments delineated by //======= KAlarm =======// comments.
 */
/*
    kmime_header_parsing.cpp

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2001-2002 the KMime authors.
    See file AUTHORS for details

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, US
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "kmime_header_parsing.h"

#include "kmime_codecs.h"
#include "kmime_util.h"
#include "kmime_warning.h"

#include <kglobal.h>
#include <kcharsets.h>

#include <qtextcodec.h>
#include <qmap.h>
#include <qcstring.h>
#include <qstringlist.h>

#include <pwd.h>
#include <ctype.h> // for isdigit
#include <cassert>

using namespace KMime;
using namespace KMime::Types;

namespace KMime {

namespace HeaderParsing {

// parse the encoded-word (scursor points to after the initial '=')
bool parseEncodedWord( const char* & scursor, const char * const send,
		       QString & result, QCString & language ) {

  // make sure the caller already did a bit of the work.
  assert( *(scursor-1) == '=' );

  //
  // STEP 1:
  // scan for the charset/language portion of the encoded-word
  //

  char ch = *scursor++;

  if ( ch != '?' ) {
    kdDebug() << "first" << endl;
    KMIME_WARN_PREMATURE_END_OF(EncodedWord);
    return false;
  }

  // remember start of charset (ie. just after the initial "=?") and
  // language (just after the first '*') fields:
  const char * charsetStart = scursor;
  const char * languageStart = 0;

  // find delimiting '?' (and the '*' separating charset and language
  // tags, if any):
  for ( ; scursor != send ; scursor++ )
    if ( *scursor == '?')
      break;
    else if ( *scursor == '*' && !languageStart )
      languageStart = scursor + 1;

  // not found? can't be an encoded-word!
  if ( scursor == send || *scursor != '?' ) {
    kdDebug() << "second" << endl;
    KMIME_WARN_PREMATURE_END_OF(EncodedWord);
    return false;
  }

  // extract the language information, if any (if languageStart is 0,
  // language will be null, too):
  QCString maybeLanguage( languageStart, scursor - languageStart + 1 /*for NUL*/);
  // extract charset information (keep in mind: the size given to the
  // ctor is one off due to the \0 terminator):
  QCString maybeCharset( charsetStart, ( languageStart ? languageStart : scursor + 1 ) - charsetStart );

  //
  // STEP 2:
  // scan for the encoding portion of the encoded-word
  //


  // remember start of encoding (just _after_ the second '?'):
  scursor++;
  const char * encodingStart = scursor;

  // find next '?' (ending the encoding tag):
  for ( ; scursor != send ; scursor++ )
    if ( *scursor == '?' ) break;

  // not found? Can't be an encoded-word!
  if ( scursor == send || *scursor != '?' ) {
    kdDebug() << "third" << endl;
    KMIME_WARN_PREMATURE_END_OF(EncodedWord);
    return false;
  }

  // extract the encoding information:
  QCString maybeEncoding( encodingStart, scursor - encodingStart + 1 );


  kdDebug() << "parseEncodedWord: found charset == \"" << maybeCharset
	    << "\"; language == \"" << maybeLanguage
	    << "\"; encoding == \"" << maybeEncoding << "\"" << endl;

  //
  // STEP 3:
  // scan for encoded-text portion of encoded-word
  //


  // remember start of encoded-text (just after the third '?'):
  scursor++;
  const char * encodedTextStart = scursor;

  // find next '?' (ending the encoded-text):
  for ( ; scursor != send ; scursor++ )
    if ( *scursor == '?' ) break;

  // not found? Can't be an encoded-word!
  // ### maybe evaluate it nonetheless if the rest is OK?
  if ( scursor == send || *scursor != '?' ) {
    kdDebug() << "fourth" << endl;
    KMIME_WARN_PREMATURE_END_OF(EncodedWord);
    return false;
  }
  scursor++;
  // check for trailing '=':
  if ( scursor == send || *scursor != '=' ) {
    kdDebug() << "fifth" << endl;
    KMIME_WARN_PREMATURE_END_OF(EncodedWord);
    return false;
  }
  scursor++;

  // set end sentinel for encoded-text:
  const char * const encodedTextEnd = scursor - 2;

  //
  // STEP 4:
  // setup decoders for the transfer encoding and the charset
  //


  // try if there's a codec for the encoding found:
  Codec * codec = Codec::codecForName( maybeEncoding );
  if ( !codec ) {
    KMIME_WARN_UNKNOWN(Encoding,maybeEncoding);
    return false;
  }

  // get an instance of a corresponding decoder:
  Decoder * dec = codec->makeDecoder();
  assert( dec );

  // try if there's a (text)codec for the charset found:
  bool matchOK = false;
  QTextCodec
    *textCodec = KGlobal::charsets()->codecForName( maybeCharset, matchOK );

  if ( !matchOK || !textCodec ) {
    KMIME_WARN_UNKNOWN(Charset,maybeCharset);
    delete dec;
    return false;
  };

//======= KAlarm: mimeName() not in Qt2 =======//
//  kdDebug() << "mimeName(): \"" << textCodec->mimeName() << "\"" << endl;
//======= KAlarm end =======//

  // allocate a temporary buffer to store the 8bit text:
  int encodedTextLength = encodedTextEnd - encodedTextStart;
  QByteArray buffer( codec->maxDecodedSizeFor( encodedTextLength ) );
  QByteArray::Iterator bit = buffer.begin();
  QByteArray::ConstIterator bend = buffer.end();

  //
  // STEP 5:
  // do the actual decoding
  //

  if ( !dec->decode( encodedTextStart, encodedTextEnd, bit, bend ) )
    KMIME_WARN << codec->name() << " codec lies about it's maxDecodedSizeFor( "
	       << encodedTextLength << " )\nresult may be truncated" << endl;

  result = textCodec->toUnicode( buffer.begin(), bit - buffer.begin() );

  kdDebug() << "result now: \"" << result << "\"" << endl;
  // cleanup:
  delete dec;
  language = maybeLanguage;

  return true;
}

static inline void eatWhiteSpace( const char* & scursor, const char * const send ) {
  while ( scursor != send
	  && ( *scursor == ' ' || *scursor == '\n' ||
	       *scursor == '\t' || *scursor == '\r' ) )
    scursor++;
}

bool parseAtom( const char * & scursor, const char * const send,
		QString & result, bool allow8Bit )
{
  QPair<const char*,int> maybeResult;

  if ( parseAtom( scursor, send, maybeResult, allow8Bit ) ) {
    result += QString::fromLatin1( maybeResult.first, maybeResult.second );
    return true;
  }

  return false;
}

bool parseAtom( const char * & scursor, const char * const send,
		QPair<const char*,int> & result, bool allow8Bit ) {
  bool success = false;
  const char * start = scursor;

  while ( scursor != send ) {
    signed char ch = *scursor++;
    if ( ch > 0 && isAText(ch) ) {
      // AText: OK
      success = true;
    } else if ( allow8Bit && ch < 0 ) {
      // 8bit char: not OK, but be tolerant.
      KMIME_WARN_8BIT(ch);
      success = true;
    } else {
      // CTL or special - marking the end of the atom:
      // re-set sursor to point to the offending
      // char and return:
      scursor--;
      break;
    }
  }
  result.first = start;
  result.second = scursor - start;
  return success;
}

bool parseToken( const char * & scursor, const char * const send,
		 QString & result, bool allow8Bit )
{
  QPair<const char*,int> maybeResult;

  if ( parseToken( scursor, send, maybeResult, allow8Bit ) ) {
    result += QString::fromLatin1( maybeResult.first, maybeResult.second );
    return true;
  }

  return false;
}

bool parseToken( const char * & scursor, const char * const send,
		 QPair<const char*,int> & result, bool allow8Bit )
{
  bool success = false;
  const char * start = scursor;

  while ( scursor != send ) {
    char ch = *scursor++;
    if ( ch > 0 && isTText(ch) ) {
      // TText: OK
      success = true;
    } else if ( allow8Bit && ch < 0 ) {
      // 8bit char: not OK, but be tolerant.
      KMIME_WARN_8BIT(ch);
      success = true;
    } else {
      // CTL or tspecial - marking the end of the atom:
      // re-set sursor to point to the offending
      // char and return:
      scursor--;
      break;
    }
  }
  result.first = start;
  result.second = scursor - start;
  return success;
}

#define READ_ch_OR_FAIL if ( scursor == send ) { \
                          KMIME_WARN_PREMATURE_END_OF(GenericQuotedString); \
                          return false; \
                        } else { \
                          ch = *scursor++; \
		        }

// known issues:
//
// - doesn't handle quoted CRLF

bool parseGenericQuotedString( const char* & scursor, const char * const send,
			       QString & result, bool isCRLF,
			       const char openChar, const char closeChar )
{
  char ch;
  // We are in a quoted-string or domain-literal or comment and the
  // cursor points to the first char after the openChar.
  // We will apply unfolding and quoted-pair removal.
  // We return when we either encounter the end or unescaped openChar
  // or closeChar.

  assert( *(scursor-1) == openChar || *(scursor-1) == closeChar );

  while ( scursor != send ) {
    ch = *scursor++;

    if ( ch == closeChar || ch == openChar ) {
      // end of quoted-string or another opening char:
      // let caller decide what to do.
      return true;
    }

    switch( ch ) {
    case '\\':      // quoted-pair
      // misses "\" CRLF LWSP-char handling, see rfc822, 3.4.5
      READ_ch_OR_FAIL;
      KMIME_WARN_IF_8BIT(ch);
      result += QChar(ch);
      break;
    case '\r':
      // ###
      // The case of lonely '\r' is easy to solve, as they're
      // not part of Unix Line-ending conventions.
      // But I see a problem if we are given Unix-native
      // line-ending-mails, where we cannot determine anymore
      // whether a given '\n' was part of a CRLF or was occuring
      // on it's own.
      READ_ch_OR_FAIL;
      if ( ch != '\n' ) {
	// CR on it's own...
	KMIME_WARN_LONE(CR);
	result += QChar('\r');
	scursor--; // points to after the '\r' again
      } else {
	// CRLF encountered.
	// lookahead: check for folding
	READ_ch_OR_FAIL;
	if ( ch == ' ' || ch == '\t' ) {
	  // correct folding;
	  // position cursor behind the CRLF WSP (unfolding)
	  // and add the WSP to the result
	  result += QChar(ch);
	} else {
	  // this is the "shouldn't happen"-case. There is a CRLF
	  // inside a quoted-string without it being part of FWS.
	  // We take it verbatim.
	  KMIME_WARN_NON_FOLDING(CRLF);
	  result += "\r\n";
	  // the cursor is decremented again, so's we need not
	  // duplicate the whole switch here. "ch" could've been
	  // everything (incl. openChar or closeChar).
	  scursor--;
	}
      }
      break;
    case '\n':
      // Note: CRLF has been handled above already!
      // ### LF needs special treatment, depending on whether isCRLF
      // is true (we can be sure a lonely '\n' was meant this way) or
      // false ('\n' alone could have meant LF or CRLF in the original
      // message. This parser assumes CRLF iff the LF is followed by
      // either WSP (folding) or NULL (premature end of quoted-string;
      // Should be fixed, since NULL is allowed as per rfc822).
      READ_ch_OR_FAIL;
      if ( !isCRLF && ( ch == ' ' || ch == '\t' ) ) {
	// folding
	// correct folding
	result += QChar(ch);
      } else {
	// non-folding
	KMIME_WARN_LONE(LF);
	result += QChar('\n');
	// pos is decremented, so's we need not duplicate the whole
	// switch here. ch could've been everything (incl. <">, "\").
	scursor--;
      }
      break;
    default:
      KMIME_WARN_IF_8BIT(ch);
      result += QChar(ch);
    }
  }

  return false;
}

// known issues:
//
// - doesn't handle encoded-word inside comments.

bool parseComment( const char* & scursor, const char * const send,
		   QString & result, bool isCRLF, bool reallySave )
{
  int commentNestingDepth = 1;
  const char * afterLastClosingParenPos = 0;
  QString maybeCmnt;
  const char * oldscursor = scursor;

  assert( *(scursor-1) == '(' );

  while ( commentNestingDepth ) {
    QString cmntPart;
    if ( parseGenericQuotedString( scursor, send, cmntPart, isCRLF, '(', ')' ) ) {
      assert( *(scursor-1) == ')' || *(scursor-1) == '(' );
      // see the kdoc for above function for the possible conditions
      // we have to check:
      switch ( *(scursor-1) ) {
      case ')':
	if ( reallySave ) {
	  // add the chunk that's now surely inside the comment.
	  result += maybeCmnt;
	  result += cmntPart;
	  if ( commentNestingDepth > 1 ) // don't add the outermost ')'...
	    result += QChar(')');
	  maybeCmnt = QString::null;
	}
	afterLastClosingParenPos = scursor;
	--commentNestingDepth;
	break;
      case '(':
	if ( reallySave ) {
	  // don't add to "result" yet, because we might find that we
	  // are already outside the (broken) comment...
	  maybeCmnt += cmntPart;
	  maybeCmnt += QChar('(');
	}
	++commentNestingDepth;
	break;
      default: assert( 0 );
      } // switch
    } else {
      // !parseGenericQuotedString, ie. premature end
      if ( afterLastClosingParenPos )
	scursor = afterLastClosingParenPos;
      else
	scursor = oldscursor;
      return false;
    }
  } // while

  return true;
}


// known issues: none.

bool parsePhrase( const char* & scursor, const char * const send,
		  QString & result, bool isCRLF )
{
  enum { None, Phrase, Atom, EncodedWord, QuotedString } found = None;
  QString tmp;
  QCString lang;
  const char * successfullyParsed = 0;
  // only used by the encoded-word branch
  const char * oldscursor;
  // used to suppress whitespace between adjacent encoded-words
  // (rfc2047, 6.2):
  bool lastWasEncodedWord = false;

  while ( scursor != send ) {
    char ch = *scursor++;
    switch ( ch ) {
    case '"': // quoted-string
      tmp = QString::null;
      if ( parseGenericQuotedString( scursor, send, tmp, isCRLF, '"', '"' ) ) {
	successfullyParsed = scursor;
	assert( *(scursor-1) == '"' );
	switch ( found ) {
	case None:
	  found = QuotedString;
	  break;
	case Phrase:
	case Atom:
	case EncodedWord:
	case QuotedString:
	  found = Phrase;
	  result += QChar(' '); // rfc822, 3.4.4
	  break;
	default:
	  assert( 0 );
	}
	lastWasEncodedWord = false;
	result += tmp;
      } else {
	// premature end of quoted string.
	// What to do? Return leading '"' as special? Return as quoted-string?
	// We do the latter if we already found something, else signal failure.
	if ( found == None ) {
	  return false;
	} else {
	  result += QChar(' '); // rfc822, 3.4.4
	  result += tmp;
	  return true;
	}
      }
      break;
    case '(': // comment
      // parse it, but ignore content:
      tmp = QString::null;
      if ( parseComment( scursor, send, tmp, isCRLF,
			 false /*don't bother with the content*/ ) ) {
	successfullyParsed = scursor;
	lastWasEncodedWord = false; // strictly interpreting rfc2047, 6.2
      } else {
	if ( found == None )
	  return false;
	else {
	  scursor = successfullyParsed;
	  return true;
	}
      }
      break;
    case '=': // encoded-word
      tmp = QString::null;
      oldscursor = scursor;
      lang = 0;
      if ( parseEncodedWord( scursor, send, tmp, lang ) ) {
	successfullyParsed = scursor;
	switch ( found ) {
	case None:
	  found = EncodedWord;
	  break;
	case Phrase:
	case EncodedWord:
	case Atom:
	case QuotedString:
	  if ( !lastWasEncodedWord )
	    result += QChar(' '); // rfc822, 3.4.4
	  found = Phrase;
	  break;
	default: assert( 0 );
	}
	lastWasEncodedWord = true;
	result += tmp;
	break;
      } else
	// parse as atom:
	scursor = oldscursor;
      // fall though...

    default: //atom
      tmp = QString::null;
      scursor--;
      if ( parseAtom( scursor, send, tmp, true /* allow 8bit */ ) ) {
	successfullyParsed = scursor;
	switch ( found ) {
	case None:
	  found = Atom;
	  break;
	case Phrase:
	case Atom:
	case EncodedWord:
	case QuotedString:
	  found = Phrase;
	  result += QChar(' '); // rfc822, 3.4.4
	  break;
	default:
	  assert( 0 );
	}
	lastWasEncodedWord = false;
	result += tmp;
      } else {
	if ( found == None )
	  return false;
	else {
	  scursor = successfullyParsed;
	  return true;
	}
      }
    }
    eatWhiteSpace( scursor, send );
  }

  return ( found != None );
}


bool parseDotAtom( const char* & scursor, const char * const send,
		   QString & result, bool isCRLF )
{
  // always points to just after the last atom parsed:
  const char * successfullyParsed;

  QString tmp;
  if ( !parseAtom( scursor, send, tmp, false /* no 8bit */ ) )
    return false;
  result += tmp;
  successfullyParsed = scursor;

  while ( scursor != send ) {
    eatCFWS( scursor, send, isCRLF );

    // end of header or no '.' -> return
    if ( scursor == send || *scursor != '.' ) return true;
    scursor++; // eat '.'

    eatCFWS( scursor, send, isCRLF );

    if ( scursor == send || !isAText( *scursor ) ) {
      // end of header or no AText, but this time following a '.'!:
      // reset cursor to just after last successfully parsed char and
      // return:
      scursor = successfullyParsed;
      return true;
    }

    // try to parse the next atom:
    QString maybeAtom;
    if ( !parseAtom( scursor, send, maybeAtom, false /*no 8bit*/ ) ) {
      scursor = successfullyParsed;
      return true;
    }

    result += QChar('.');
    result += maybeAtom;
    successfullyParsed = scursor;
  }

  scursor = successfullyParsed;
  return true;
}


void eatCFWS( const char* & scursor, const char * const send, bool isCRLF ) {
  QString dummy;

  while ( scursor != send ) {
    const char * oldscursor = scursor;

    char ch = *scursor++;

    switch( ch ) {
    case ' ':
    case '\t': // whitespace
    case '\r':
    case '\n': // folding
      continue;

    case '(': // comment
      if ( parseComment( scursor, send, dummy, isCRLF, false /*don't save*/ ) )
	continue;
      scursor = oldscursor;
      return;

    default:
      scursor = oldscursor;
      return;
    }

  }
}

bool parseDomain( const char* & scursor, const char * const send,
		  QString & result, bool isCRLF ) {
  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send ) return false;

  // domain := dot-atom / domain-literal / atom *("." atom)
  //
  // equivalent to:
  // domain = dot-atom / domain-literal,
  // since parseDotAtom does allow CFWS between atoms and dots

  if ( *scursor == '[' ) {
    // domain-literal:
    QString maybeDomainLiteral;
    // eat '[':
    scursor++;
    while ( parseGenericQuotedString( scursor, send, maybeDomainLiteral,
				      isCRLF, '[', ']' ) ) {
      if ( scursor == send ) {
	// end of header: check for closing ']':
	if ( *(scursor-1) == ']' ) {
	  // OK, last char was ']':
	  result = maybeDomainLiteral;
	  return true;
	} else {
	  // not OK, domain-literal wasn't closed:
	  return false;
	}
      }
      // we hit openChar in parseGenericQuotedString.
      // include it in maybeDomainLiteral and keep on parsing:
      if ( *(scursor-1) == '[' ) {
	maybeDomainLiteral += QChar('[');
	continue;
      }
      // OK, real end of domain-literal:
      result = maybeDomainLiteral;
      return true;
    }
  } else {
    // dot-atom:
    QString maybeDotAtom;
    if ( parseDotAtom( scursor, send, maybeDotAtom, isCRLF ) ) {
      result = maybeDotAtom;
      return true;
    }
  }
  return false;
}

bool parseObsRoute( const char* & scursor, const char* const send,
		    QStringList & result, bool isCRLF, bool save ) {
  while ( scursor != send ) {
    eatCFWS( scursor, send, isCRLF );
    if ( scursor == send ) return false;

    // empty entry:
    if ( *scursor == ',' ) {
      scursor++;
      if ( save ) result.append( QString::null );
      continue;
    }

    // empty entry ending the list:
    if ( *scursor == ':' ) {
      scursor++;
      if ( save ) result.append( QString::null );
      return true;
    }

    // each non-empty entry must begin with '@':
    if ( *scursor != '@' )
      return false;
    else
      scursor++;

    QString maybeDomain;
    if ( !parseDomain( scursor, send, maybeDomain, isCRLF ) ) return false;
    if ( save ) result.append( maybeDomain );

    // eat the following (optional) comma:
    eatCFWS( scursor, send, isCRLF );
    if ( scursor == send ) return false;
    if ( *scursor == ':' ) { scursor++; return true; }
    if ( *scursor == ',' ) scursor++;

  }

  return false;
}

bool parseAddrSpec( const char* & scursor, const char * const send,
		    AddrSpec & result, bool isCRLF ) {
  //
  // STEP 1:
  // local-part := dot-atom / quoted-string / word *("." word)
  //
  // this is equivalent to:
  // local-part := word *("." word)

  QString maybeLocalPart;
  QString tmp;

  while ( scursor != send ) {
    // first, eat any whitespace
    eatCFWS( scursor, send, isCRLF );

    char ch = *scursor++;
    switch ( ch ) {
    case '.': // dot
      maybeLocalPart += QChar('.');
      break;

    case '@':
      goto SAW_AT_SIGN;
      break;

    case '"': // quoted-string
      tmp = QString::null;
      if ( parseGenericQuotedString( scursor, send, tmp, isCRLF, '"', '"' ) )
	maybeLocalPart += tmp;
      else
	return false;
      break;

    default: // atom
      scursor--; // re-set scursor to point to ch again
      tmp = QString::null;
      if ( parseAtom( scursor, send, tmp, false /* no 8bit */ ) )
	maybeLocalPart += tmp;
      else
	return false; // parseAtom can only fail if the first char is non-atext.
      break;
    }
  }

  return false;


  //
  // STEP 2:
  // domain
  //

SAW_AT_SIGN:

  assert( *(scursor-1) == '@' );

  QString maybeDomain;
  if ( !parseDomain( scursor, send, maybeDomain, isCRLF ) )
    return false;

  result.localPart = maybeLocalPart;
  result.domain = maybeDomain;

  return true;
}


bool parseAngleAddr( const char* & scursor, const char * const send,
		     AddrSpec & result, bool isCRLF ) {
  // first, we need an opening angle bracket:
  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send || *scursor != '<' ) return false;
  scursor++; // eat '<'

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send ) return false;

  if ( *scursor == '@' || *scursor == ',' ) {
    // obs-route: parse, but ignore:
    KMIME_WARN << "obsolete source route found! ignoring." << endl;
    QStringList dummy;
    if ( !parseObsRoute( scursor, send, dummy,
			 isCRLF, false /* don't save */ ) )
      return false;
    // angle-addr isn't complete until after the '>':
    if ( scursor == send ) return false;
  }

  // parse addr-spec:
  AddrSpec maybeAddrSpec;
  if ( !parseAddrSpec( scursor, send, maybeAddrSpec, isCRLF ) ) return false;

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send || *scursor != '>' ) return false;
  scursor++;

  result = maybeAddrSpec;
  return true;

}

bool parseMailbox( const char* & scursor, const char * const send,
		   Mailbox & result, bool isCRLF ) {

  // rfc:
  // mailbox := addr-spec / ([ display-name ] angle-addr)
  // us:
  // mailbox := addr-spec / ([ display-name ] angle-addr)
  //                      / (angle-addr "(" display-name ")")

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send ) return false;

  AddrSpec maybeAddrSpec;

  // first, try if it's a vanilla addr-spec:
  const char * oldscursor = scursor;
  if ( parseAddrSpec( scursor, send, maybeAddrSpec, isCRLF ) ) {
    result.displayName = QString::null;
    result.addrSpec = maybeAddrSpec;
    return true;
  }
  scursor = oldscursor;

  // second, see if there's a display-name:
  QString maybeDisplayName;
  if ( !parsePhrase( scursor, send, maybeDisplayName, isCRLF ) ) {
    // failed: reset cursor, note absent display-name
    maybeDisplayName = QString::null;
    scursor = oldscursor;
  } else {
    // succeeded: eat CFWS
    eatCFWS( scursor, send, isCRLF );
    if ( scursor == send ) return false;
  }

  // third, parse the angle-addr:
  if ( !parseAngleAddr( scursor, send, maybeAddrSpec, isCRLF ) )
    return false;

  if ( maybeDisplayName.isNull() ) {
    // check for the obsolete form of display-name (as comment):
    eatWhiteSpace( scursor, send );
    if ( scursor != send && *scursor == '(' ) {
      scursor++;
      if ( !parseComment( scursor, send, maybeDisplayName, isCRLF, true /*keep*/ ) )
	return false;
    }
  }

  result.displayName = maybeDisplayName;
  result.addrSpec = maybeAddrSpec;
  return true;
}

bool parseGroup( const char* & scursor, const char * const send,
		 Address & result, bool isCRLF ) {
  // group         := display-name ":" [ mailbox-list / CFWS ] ";" [CFWS]
  //
  // equivalent to:
  // group   := display-name ":" [ obs-mbox-list ] ";"

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send ) return false;

  // get display-name:
  QString maybeDisplayName;
  if ( !parsePhrase( scursor, send, maybeDisplayName, isCRLF ) )
    return false;

  // get ":":
  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send || *scursor != ':' ) return false;

  result.displayName = maybeDisplayName;

  // get obs-mbox-list (may contain empty entries):
  scursor++;
  while ( scursor != send ) {
    eatCFWS( scursor, send, isCRLF );
    if ( scursor == send ) return false;

    // empty entry:
    if ( *scursor == ',' ) { scursor++; continue; }

    // empty entry ending the list:
    if ( *scursor == ';' ) { scursor++; return true; }

    Mailbox maybeMailbox;
    if ( !parseMailbox( scursor, send, maybeMailbox, isCRLF ) )
      return false;
    result.mailboxList.append( maybeMailbox );

    eatCFWS( scursor, send, isCRLF );
    // premature end:
    if ( scursor == send ) return false;
    // regular end of the list:
    if ( *scursor == ';' ) { scursor++; return true; }
    // eat regular list entry separator:
    if ( *scursor == ',' ) scursor++;
  }
  return false;
}

//======= KAlarm: Allow a local user name to be specified =======//
bool parseUserName( const char* & scursor, const char * const send,
                    QString & result, bool isCRLF ) {

  QString maybeLocalPart;
  QString tmp;

  while ( scursor != send ) {
    // first, eat any whitespace
    eatCFWS( scursor, send, isCRLF );

    char ch = *scursor++;
    switch ( ch ) {
    case '.': // dot
    case '@':
    case '"': // quoted-string
      return false;

    default: // atom
      scursor--; // re-set scursor to point to ch again
      tmp = QString::null;
      if ( parseAtom( scursor, send, result, false /* no 8bit */ ) ) {
        if (getpwnam(result.local8Bit()))
          return true;
      }
      return false; // parseAtom can only fail if the first char is non-atext.
    }
  }
  return false;
}
//======= KAlarm end =======//

bool parseAddress( const char* & scursor, const char * const send,
		   Address & result, bool isCRLF ) {
  // address       := mailbox / group

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send ) return false;

  // first try if it's a single mailbox:
  Mailbox maybeMailbox;
  const char * oldscursor = scursor;
  if ( parseMailbox( scursor, send, maybeMailbox, isCRLF ) ) {
    // yes, it is:
    result.displayName = QString::null;
    result.mailboxList.append( maybeMailbox );
    return true;
  }
  scursor = oldscursor;

//======= KAlarm: Allow a local user name to be specified =======//
  // no, it's not a single mailbox. Try if it's a local user name:
  QString maybeUserName;
  if ( parseUserName( scursor, send, maybeUserName, isCRLF ) ) {
    // yes, it is:
    maybeMailbox.displayName = QString::null;
    maybeMailbox.addrSpec.localPart = maybeUserName;
    maybeMailbox.addrSpec.domain = QString::null;
    result.displayName = QString::null;
    result.mailboxList.append( maybeMailbox );
    return true;
  }
  scursor = oldscursor;
//======= KAlarm end =======//

  Address maybeAddress;

  // no, it's not a single mailbox. Try if it's a group:
  if ( !parseGroup( scursor, send, maybeAddress, isCRLF ) )
//======= KAlarm: Reinstate original scursor on error return =======//
  {
    scursor = oldscursor;
    return false;
  }
//======= KAlarm end =======//

  result = maybeAddress;
  return true;
}

bool parseAddressList( const char* & scursor, const char * const send,
		       QValueList<Address> & result, bool isCRLF ) {
  while ( scursor != send ) {
    eatCFWS( scursor, send, isCRLF );
    // end of header: this is OK.
    if ( scursor == send ) return true;
    // empty entry: ignore:
//======= KAlarm: Allow ';' address separator =======//
    if ( *scursor == ',' || *scursor == ';' ) { scursor++; continue; }
//======= KAlarm end =======//

    // parse one entry
    Address maybeAddress;
    if ( !parseAddress( scursor, send, maybeAddress, isCRLF ) ) return false;
    result.append( maybeAddress );

    eatCFWS( scursor, send, isCRLF );
    // end of header: this is OK.
    if ( scursor == send ) return true;
    // comma separating entries: eat it.
//======= KAlarm: Allow ';' address separator =======//
    if ( *scursor == ',' || *scursor == ';' ) scursor++;
//======= KAlarm end =======//
  }
  return true;
}

}; // namespace HeaderParsing

}; // namespace KMime
