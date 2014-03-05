
#include "libws_compat.h"
#include <assert.h>
#include <string.h>

uint64_t libws_ntoh64(const uint64_t input)
{
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;

	data[0] = (uint8_t)(input >> 56);
	data[1] = (uint8_t)(input >> 48);
	data[2] = (uint8_t)(input >> 40);
	data[3] = (uint8_t)(input >> 32);
	data[4] = (uint8_t)(input >> 24);
	data[5] = (uint8_t)(input >> 16);
	data[6] = (uint8_t)(input >> 8);
	data[7] = (uint8_t)(input >> 0);

    return rval;
}

uint64_t libws_hton64(const uint64_t input)
{
    return (libws_ntoh64(input));
}

char *libws_strsep(char **s, const char *del)
{
    char *d, *tok;
    assert(strlen(del) == 1);

    if (!s || !*s)
        return NULL;
    
    tok = *s;
    d = strstr(tok, del);

    if (d) 
    {
        *d = '\0';
        *s = d + 1;
    } 
    else
    {
        *s = NULL;
    }

    return tok;
}

char *ws_rtrim(char *s)
{
    size_t len = strlen(s);
    char *end = s + len - 1;

    while ((end > s) && (*end == ' ')) end--;
    end++;

    *end = 0;

    return s;
}


