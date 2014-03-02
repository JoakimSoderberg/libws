#include "libws_utf8.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

//
// Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
// MIT Licensed
//

// http://tools.ietf.org/html/rfc3629

#define UNI_SUR_HIGH_START   (uint32_t) 0xD800
#define UNI_SUR_LOW_END    (uint32_t) 0xDFFF
#define UNI_REPLACEMENT_CHAR (uint32_t) 0x0000FFFD
#define UNI_MAX_LEGAL_UTF32  (uint32_t) 0x0010FFFF

static const uint8_t trailingBytesForUTF8[256] = 
{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

static const uint32_t offsetsFromUTF8[6] = 
{
	0x00000000, 0x00003080, 0x000E2080,
	0x03C82080, 0xFA082080, 0x82082080
};

static int isLegalUTF8(const uint8_t *source, const int length)
{
	uint8_t a;
	const uint8_t *srcptr = source + length;

	switch (length) 
	{
		default: return 0;
		// Everything else falls through when "true"...
		// RFC3629 makes 5 & 6 bytes UTF-8 illegal
		//case 6: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return 0;
		//case 5: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return 0;
		case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return 0;
		case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return 0;
		case 2: if ((a = (*--srcptr)) > 0xBF) return 0;
		switch (*source) 
		{
			// no fall-through in this inner switch.
			case 0xE0: if (a < 0xA0) return 0; break;
			case 0xED: if (a > 0x9F) return 0; break;
			case 0xF0: if (a < 0x90) return 0; break;
			case 0xF4: if (a > 0x8F) return 0; break;
			default:   if (a < 0x80) return 0;
		}
		case 1: if (*source >= 0x80 && *source < 0xC2) return 0;
	}
	if (*source > 0xF4) return 0;
	return 1;
}

ws_utf8_parse_state_t ws_utf8_validate(const unsigned char *value, 
									size_t len, size_t *offset)
{
	unsigned int i;

	// Is the string valid UTF-8?
	for (i = 0; i < len; i++) 
	{
		uint32_t ch = 0;
		uint8_t  extrabytes = trailingBytesForUTF8[(uint8_t) value[i]];
		
		if (offset)
			*offset = i;

		if (extrabytes + i >= len)
		{
			// We need more data.
			return (ws_utf8_parse_state_t)(extrabytes + (i - len + 1));
		}

		if (isLegalUTF8((uint8_t *)(value + i), extrabytes + 1) == 0) 
			return WS_UTF8_FAILURE;

		switch (extrabytes) 
		{
			case 5 : ch += (uint8_t) value[i++]; ch <<= 6;
			case 4 : ch += (uint8_t) value[i++]; ch <<= 6;
			case 3 : ch += (uint8_t) value[i++]; ch <<= 6;
			case 2 : ch += (uint8_t) value[i++]; ch <<= 6;
			case 1 : ch += (uint8_t) value[i++]; ch <<= 6;
			case 0 : ch += (uint8_t) value[i];
		}

		ch -= offsetsFromUTF8[extrabytes];

		if (ch <= UNI_MAX_LEGAL_UTF32) 
		{
			if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END)
				return WS_UTF8_FAILURE;
		} 
		else 
		{
			return WS_UTF8_FAILURE;
		}
	}

	return WS_UTF8_SUCCESS;
}

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

static const uint8_t utf8d[] =
{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

uint32_t libws_utf8_decode(uint32_t *state, uint32_t *codep, uint8_t byte)
{
	uint32_t type = utf8d[byte];

	*codep = (*state != WS_UTF8_ACCEPT) ?
			 (byte & 0x3fu) | (*codep << 6) :
			 (0xff >> type) & (byte);

	*state = utf8d[256 + (*state) * 16 + type];
	return *state;
}

uint32_t libws_utf8_validate2(uint32_t *state, char *str, size_t len)
{
	size_t i;
	uint32_t type;

	for (i = 0; i < len; i++)
	{
		// We don't care about the codepoint, so this is
		// a simplified version of the decode function.
		type = utf8d[(uint8_t)str[i]];
		*state = utf8d[256 + (*state) * 16 + type];

		if (*state == WS_UTF8_REJECT)
			break;
	}

	return *state;
}
