#include "new_common.h"

// Why strdup breaks strings?
// backlog lcd_clearAndGoto I2C1 0x23 1 1; lcd_print I2C1 0x23 Enabled
// it got broken around 64 char
// where is buffer with [64] bytes?
char *test_strdup(const char *s)
{
    char *res;
    size_t len;

    if (s == NULL)
        return NULL;

    len = strlen(s);
    res = malloc(len + 1);
    if (res)
        memcpy(res, s, len + 1);

    return res;
}
int strIsInteger(const char *s) {
	if(s==0)
		return 0;
	if(*s == 0)
		return 0;
	while(*s) {
		if(isdigit((unsigned char)*s)==false)
			return 0;
		s++;
	}
	return 1;
}
// returns amount of space left in buffer (0=overflow happened)
int strcat_safe(char *tg, const char *src, int tgMaxLen) {
	int curOfs = 1;

	// skip
	while(*tg != 0) {
		tg++;
		curOfs++;
		if(curOfs >= tgMaxLen - 1) {
			*tg = 0;
			return 0;
		}
	}
	// copy
	while(*src != 0) {
		*tg = *src;
		src++;
		tg++;
		curOfs++;
		if(curOfs >= tgMaxLen - 1) {
			*tg = 0;
			return 0;
		}
	}
	*tg = 0;
	return tgMaxLen-curOfs;
}


int strcpy_safe_checkForChanges(char *tg, const char *src, int tgMaxLen) {
	int changesFound = 0;
	int curOfs = 0;
	// copy
	while(*src != 0) {
		if(*tg != *src) {
			changesFound++;
			*tg = *src;
		}
		src++;
		tg++;
		curOfs++;
		if(curOfs >= tgMaxLen - 1) {
			if(*tg != 0) {
				changesFound++;
				*tg = 0;
			}
			return 0;
		}
	}
	if(*tg != 0) {
		changesFound++;
		*tg = 0;
	}
	return changesFound;
}
int strcpy_safe(char *tg, const char *src, int tgMaxLen) {
	int curOfs = 0;
	// copy
	while(*src != 0) {
		*tg = *src;
		src++;
		tg++;
		curOfs++;
		if(curOfs >= tgMaxLen - 1) {
			*tg = 0;
			return 0;
		}
	}
	*tg = 0;
	return tgMaxLen-curOfs;
}

void urldecode2_safe(char *dst, const char *srcin, int maxDstLen)
{
	int curLen = 1;
        int a = 0, b = 0;
	// avoid signing issues in conversion to int for isxdigit(int c)
	const unsigned char *src = (const unsigned char *)srcin;
        while (*src) {
		if(curLen >= (maxDstLen - 1)){
			break;
		}
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                } else {
                        *dst++ = *src++;
                }
		curLen++;
        }
        *dst++ = '\0';
}

// from https://github.com/softplus/random-c
//
// converts src data to hex-strings for output, checks length: 0=fail, 1=ok
int bin2hexstr(const char *src, int srcLen, const char *dst, int maxDstLen, __uint32_t offset) {
	// check length: 8xOffset, 2 spaces, 16 hex digits w/space, 1 space, 1 space, 16 chars, /n
    // -> needs 78 characters per line (16 bytes)
    int line_size = (8 + 2 + 16*3 + 1 + 1 + 16 + 2);
    int row_count = ((srcLen-1)/16)+1;
    if (maxDstLen < row_count * line_size + 1) {
		return 0;
	}
	char *hexes, *chars, *out;
	hexes=(char *)src; chars=(char *)src; out=(char *)dst; 
    // note *out is not null-terminated until we return
	for (int i=0; i<row_count*16; i++) {
		if (i%16==0) {
			out += snprintf(out, maxDstLen-(1+out-dst), "%08X", offset + i);
		}
        if (i%8==0) *(out++)=' ';
		if (i<srcLen) { out+=snprintf(out, maxDstLen-(1+out-dst), "%02x ", (__uint8_t)*hexes++);
			} else { out+=snprintf(out, maxDstLen-(1+out-dst), "-- ");}
		if ((i+1)%16==0) {
			*(out++)=' '; 
			for (int j=0; j<16; j++) {
				if (j%8==0) *out++=' ';
				if ((chars-src)<srcLen) {
					if (*chars>=' ' && *chars<127) *out++=*chars; else *out++='.';
					chars++;
				} else *out++='-';
			}
			*out++='\n';
		}
	}
	*out='\0';
	return 1;
}
