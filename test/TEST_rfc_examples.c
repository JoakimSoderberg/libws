
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
	size_t header_len;

	if (ws_unpack_header(&header, &header_len, 
			(unsigned char *)buf, buflen))
	{
		libws_test_FAILURE("Could not unpack header\n");
		ret = -1;
	}
		
	if (compare_headers(expected_header, &header))
		return -1;

	payload = &buf[header_len];

	if (header.mask_bit)
	{
		ws_unmask_payload(header.mask, payload, header.payload_len);
	}

	if (!memcmp(payload, expected_payload, expected_payload_len))
	{
		switch (header.opcode)
		{
			case WS_OPCODE_TEXT: 
				libws_test_SUCCESS("Payload as expected: \"%s\"", payload);
				break;
			case WS_OPCODE_BINARY:
				libws_test_SUCCESS("Payload as expected: [binary]");
				break;
			case WS_OPCODE_PING:
				libws_test_SUCCESS("Payload as expected: [ping]");
				break;
			case WS_OPCODE_PONG:
				libws_test_SUCCESS("Payload as expected: [pong]");
				break;
		}
	}
	else
	{
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

	return ret;
}

int TEST_rfc_examples(int argc, char *argv[])
{
	int ret = 0;
	ws_header_t expected_header;
	ws_header_t header;
	size_t header_len;
	ws_header_t *h;

	// 
	// o  A single-frame unmasked text message
	// 
	//   *  0x81 0x05 0x48 0x65 0x6c 0x6c 0x6f (contains "Hello")
	// 
	libws_test_STATUS("\nTesting a single-frame UNMASKED text message containing \"Hello\"");
	{
		char single_frame_unmasked[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};

		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_TEXT;
		h->mask_bit = 0;
		h->payload_len = strlen("Hello");
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
		
		h = &expected_header;
		h->fin = 1;
		h->rsv1 = 0;
		h->rsv2 = 0;
		h->rsv3 = 0;
		h->opcode = WS_OPCODE_TEXT;
		h->mask_bit = 1;
		h->payload_len = strlen("Hello");
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
	{
		char frag_unmasked_part1[] = {0x01, 0x03, 0x48, 0x65, 0x6c};
		char frag_unmasked_part2[] = {0x80, 0x02, 0x6c, 0x6f};
	}

	// o  Unmasked Ping request and masked Ping response
	//
	//   *  0x89 0x05 0x48 0x65 0x6c 0x6c 0x6f (contains a body of "Hello",
	//         but the contents of the body are arbitrary)
	//
	//   *  0x8a 0x85 0x37 0xfa 0x21 0x3d 0x7f 0x9f 0x4d 0x51 0x58
	//         (contains a body of "Hello", matching the body of the ping)
	//
	{
		char unmasked_ping_request[] = {0x89, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
		char unmasked_pong_response[] = {0x8a, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
	}

	// o  256 bytes binary message in a single unmasked frame
	//
	//   *  0x82 0x7E 0x0100 [256 bytes of binary data]
	//
	{
		#define FRAME_SIZE_256 (2 + 2 + 256)
		char single_frame_unmasked_binary_256[FRAME_SIZE_256] = {0x82, 0x7E, 0x01, 0x00};
		size_t i;

	for (i = 4; i < FRAME_SIZE_256; i++) 
		single_frame_unmasked_binary_256[i] = (char)(i % 256);
	}

	// o  64KiB binary message in a single unmasked frame
	//
	//   *  0x82 0x7F 0x0000000000010000 [65536 bytes of binary data]
	//
	{
		#define FRAME_SIZE_65K (2 + 8 + 65536)
		char single_frame_unmasked_binary_65k[FRAME_SIZE_65K] = {0x82, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
		size_t i;

		for (i = 10; i < FRAME_SIZE_65K; i++)
			single_frame_unmasked_binary_65k[i] = (char)(i % 256);
	}

	return ret;
}