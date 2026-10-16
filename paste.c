#include <string.h>
#include "paste.h"

void *xmalloc(size_t);

size_t
normalizepaste(const unsigned char *data, size_t len, char **out, int bracketed)
{
	char *buf, *dst;
	size_t i;

	/* Chunk-local: CRLF and bracket markers split across X INCR chunks
	 * are not combined here. Callers must not treat this as a complete
	 * security boundary for hostile clipboard owners. */
	buf = dst = xmalloc(len + 1);
	for (i = 0; i < len; i++) {
		if (bracketed && i + 6 <= len &&
		    (!memcmp(data + i, "\033[200~", 6) ||
		    !memcmp(data + i, "\033[201~", 6))) {
			i += 5;
			continue;
		}
		if (data[i] == '\r') {
			*dst++ = '\r';
			if (i + 1 < len && data[i + 1] == '\n')
				i++;
		} else if (data[i] == '\n') {
			*dst++ = '\r';
		} else {
			*dst++ = data[i];
		}
	}
	*dst = '\0';
	*out = buf;
	return dst - buf;
}
