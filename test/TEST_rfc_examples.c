
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libws.h"
#include "libws_test_helpers.h"

int compare_headers(ws_header_t *e, ws_header_t *h)
{
	int ret = 0;

	#define CMP_HDR_VAR(name, var) \
		if (e->var != h->var) \
		{ \
			libws_test_FAILURE("	" name " 0x%x expected 0x%x", h->var, e->var); \
			ret = -1; \
		} \
		else \
		{ \
			libws_test_SUCCESS("	" name " 0x%x as expected", h->var); \
		}

	CMP_HDR_VAR("FIN", fin);
	CMP_HDR_VAR("RSV1", rsv1);
	CMP_HDR_VAR("RSV2", rsv2);
	CMP_HDR_VAR("RSV3", rsv3);
	CMP_HDR_VAR("Opcode", opcode);
	CMP_HDR_VAR("Mask bit", mask_bit);
	CMP_HDR_VAR("Payload length", payload_len);
	CMP_HDR_VAR("Mask", mask);

	return ret;
}

int do_unpack_test(char *buf, size_t buflen, 
				ws_header_t *expected_header,
				char *expected_payload, size_t expected_payload_len)
{
	char *payload = NULL;
	int ret = 0;
	ws_header_t header;
	size_t i;
	size_t header_len;
	ws_parse_state_t state;

	if ((state = ws_unpack_header(&header, &header_len, 
							(unsigned char *)buf, buflen)))
	{
		libws_test_FAILURE("Could not unpack header:");
		switch (state)
		{
			case WS_PARSE_STATE_NEED_MORE: 
				libws_test_FAILURE("\tNeed more data");
				break;
			case WS_PARSE_STATE_ERROR:
				libws_test_FAILURE("\tParse error");
				break;
			case WS_PARSE_STATE_USER_ABORT:
				libws_test_FAILURE("\tUser abort");
				break;
		}
		ret |= -1;
	}

	if (compare_headers(expected_header, &header))
		return -1;

	// Make sure we copy and null terminate the payload.
	payload = (char *)calloc(header.payload_len + 1, sizeof(char));
	memcpy(payload, &buf[header_len], header.payload_len);
	payload[header.payload_len] = '\0';

	if (header.mask_bit)
	{
		ws_unmask_payload(header.mask, payload, header.payload_len);
	}

	if (!memcmp(payload, expected_payload, expected_payload_len))
	{
		switch (header.opcode)
		{
			case WS_OPCODE_TEXT: 
				libws_test_SUCCESS("Payload as expected: [text] \"%s\"", payload);
				break;
			case WS_OPCODE_BINARY:
				libws_test_SUCCESS("Payload as expected: [binary] %u bytes", header.payload_len);
				break;
			case WS_OPCODE_PING:
				libws_test_SUCCESS("Payload as expected: [ping] %u bytes", header.payload_len);
				break;
			case WS_OPCODE_PONG:
				libws_test_SUCCESS("Payload as expected: [pong] %u bytes", header.payload_len);
				break;
			case WS_OPCODE_CONTINUATION:
				libws_test_SUCCESS("Payload as expected. [continuation] \"%s\"", payload);
				break;
			default:
				libws_test_FAILURE("Unexpected opcode %d", header.opcode);
				ret |= -1;
				break;
		}
	}
	else
	{
		ret |= -1;
		switch (header.opcode)
		{
			case WS_OPCODE_TEXT:
				libws_test_FAILURE("Invalid payload: \"%s\", expected: \"%s\"", 
									payload, expected_payload);
				break;
			default:
				libws_test_FAILURE("Invalid payload");
				break;
		}
	}

	free(payload);

	return ret;
}

int TEST_rfc_examples(int argc, char *argv[])
{
	int ret = 0;
	ws_header_t expected_header;
	ws_header_t header;
	size_t header_len;
	ws_header_t *h = NULL;

	libws_test_HEADLINE("TEST_rfc_examples");

	// 
	// o  A single-frame unmasked text message
	// 
	//   *  0x81 0x05 0x48 0x65 0x6c 0x6c 0x6f (contains "Hello")
	// 
	libws_test_STATUS("\nTesting a single-frame UNMASKED text message containing \"Hello\"");
	{
		char single_frame_unmasked[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
		
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_TEXT;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)strlen("Hello");
		h->mask = 0;

		ret |= do_unpack_test(single_frame_unmasked, 
					sizeof(single_frame_unmasked),
					&expected_header,
					"Hello", strlen("Hello"));
	}

	// o  A single-frame masked text message
	// 
	//   *  0x81 0x85 0x37 0xfa 0x21 0x3d 0x7f 0x9f 0x4d 0x51 0x58
	//      (contains "Hello")
	//
	libws_test_STATUS("\nTesting a single-frame MASKED text message containing \"Hello\"");
	{
		char single_frame_masked[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
		
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_TEXT;
		h->mask_bit = 1;
		h->payload_len = (uint64_t)strlen("Hello");
		h->mask = (*((uint32_t *)&single_frame_masked[2]));

		ret |= do_unpack_test(single_frame_masked, 
					sizeof(single_frame_masked),
					&expected_header,
					"Hello", strlen("Hello"));
	}

	// o  A fragmented unmasked text message
	// 
	//   *  0x01 0x03 0x48 0x65 0x6c (contains "Hel")
	// 
	//   *  0x80 0x02 0x6c 0x6f (contains "lo")
	// 
	libws_test_STATUS("\nTesting a fragmented UNMASKED text message containing \"Hel\" and \"lo\"");
	{
		char frag_unmasked_part1[] = {0x01, 0x03, 0x48, 0x65, 0x6c};
		char frag_unmasked_part2[] = {0x80, 0x02, 0x6c, 0x6f};
		
		libws_test_STATUS("[First fragment]:");
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 0;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_TEXT;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)strlen("Hel");
		h->mask = 0;

		ret |= do_unpack_test(frag_unmasked_part1, 
					sizeof(frag_unmasked_part1),
					&expected_header,
					"Hel", strlen("Hel"));

		libws_test_STATUS("[Second fragment]:");
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_CONTINUATION;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)strlen("lo");
		h->mask = 0;

		ret |= do_unpack_test(frag_unmasked_part2, 
					sizeof(frag_unmasked_part2),
					&expected_header,
					"lo", strlen("lo"));
	}

	// o  Unmasked Ping request and masked Ping response
	//
	//   *  0x89 0x05 0x48 0x65 0x6c 0x6c 0x6f (contains a body of "Hello",
	//         but the contents of the body are arbitrary)
	//
	//   *  0x8a 0x85 0x37 0xfa 0x21 0x3d 0x7f 0x9f 0x4d 0x51 0x58
	//         (contains a body of "Hello", matching the body of the ping)
	//
	libws_test_STATUS("\nUnmasked Ping request and masked Ping response");
	{
		char unmasked_ping_request[] = {0x89, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
		char masked_pong_response[] = {0x8a, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};

		libws_test_STATUS("[Ping]:");
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_PING;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)strlen("Hello");
		h->mask = 0;

		ret |= do_unpack_test(unmasked_ping_request, 
					sizeof(unmasked_ping_request),
					&expected_header,
					"Hello", strlen("Hello"));

		libws_test_STATUS("[Pong]:");
		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_PONG;
		h->mask_bit = 1;
		h->payload_len = (uint64_t)strlen("Hello");
		h->mask = (*((uint32_t *)&masked_pong_response[2]));

		ret |= do_unpack_test(masked_pong_response, 
					sizeof(masked_pong_response),
					&expected_header,
					"Hello", strlen("Hello"));	
	}

	// o  256 bytes binary message in a single unmasked frame
	//
	//   *  0x82 0x7E 0x0100 [256 bytes of binary data]
	//
	libws_test_STATUS("\n256 bytes binary message in a single unmasked frame");
	{
		char single_frame_unmasked_binary_256[4 + 256] = {0x82, 0x7E, 0x01, 0x00};
		char expected_payload[256];
		size_t i;

		for (i = 0; i < 256; i++) 
		{
			expected_payload[i] = (char)(i % 255);
		}

		memcpy(&single_frame_unmasked_binary_256[4], expected_payload, 256);

		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_BINARY;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)256;
		h->mask = 0;

		ret |= do_unpack_test(single_frame_unmasked_binary_256, 
					sizeof(single_frame_unmasked_binary_256),
					&expected_header,
					expected_payload, 256);
	}

	// o  64KiB binary message in a single unmasked frame
	//
	//   *  0x82 0x7F 0x0000000000010000 [65536 bytes of binary data]
	//
	libws_test_STATUS("\n64KiB binary message in a single unmasked frame");
	{
		char single_frame_unmasked_binary_65k[2 + 8 + 65536] = {0x82, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
		char expected_payload[65536];		
		size_t i;

		for (i = 0; i < 65536; i++)
			expected_payload[i] = (char)(i % 255);

		memcpy(&single_frame_unmasked_binary_65k[2 + 8], expected_payload, 65536);

		memset(&expected_header, 0, sizeof(ws_header_t));
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_BINARY;
		h->mask_bit = 0;
		h->payload_len = (uint64_t)65536;
		h->mask = 0;

		ret |= do_unpack_test(single_frame_unmasked_binary_65k, 
					sizeof(single_frame_unmasked_binary_65k),
					&expected_header,
					expected_payload, 65536);
	}

	return ret;
}

