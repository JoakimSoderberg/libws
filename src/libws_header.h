#ifndef __LIBWS_HEADER_H__
#define __LIBWS_HEADER_H__

#include <stdio.h>
#include <inttypes.h>
#include "libws_types.h"

///
/// Gets the name of an opcode as a string.
///
/// @param[in] opcode The opcode.
///
/// @returns NULL on failure, string otherwise.
///
const char *ws_opcode_str(ws_opcode_t opcode);

///
/// Unpacks a websocket header from a byte buffer into
/// a header struct.
///
/// @param[out]	h 			A pointer to a header struct to store the result in.
/// @param[out] header_len 	The size of the header in bytes.
/// @param[in]	b 			The buffer that should be parsed.
/// @param[in]  len 		The number of bytes the buffer contains.
///
/// @returns A parse state. WS_PARSE_STATE_SUCCESS if the header was
///			 parsed successfully. WS_PARSE_STATE_NEED_MORE if more
///			 data is needed.
///
ws_parse_state_t ws_unpack_header(ws_header_t *h, size_t *header_len,
					const unsigned char *b, size_t len);

///
/// Packs a websocket header struct into a network byte order
/// octet stream.
///
/// @param[in]	h 			A pointer to the header to pack.
/// @param[in]	b 			The byte buffer to pack the header in. 
///							Must be at least #WS_HDR_MAX_SIZE big.
/// @param[in]	len 		The size of the buffer.
/// @param[out] header_len	Will contain the final size of the packed header.
///
void ws_pack_header(ws_header_t *h, uint8_t *b, size_t len, size_t *header_len);



#endif // __LIBWS_HEADER_H__
