/* TR46/UTS#46 tests
 *
 * (C)2016 Tim Ruehsen
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

// includes for libicu
#include <unicode/uversion.h>
#include <unicode/ustring.h>
#include <unicode/uidna.h>
#include <unicode/ucnv.h>

// includes for libunistring
#include <unicase.h>
#include <unistr.h>

// includes for libidn2
#include <idn2.h>

#define _U __attribute__ ((unused))
#define countof(a) (sizeof(a)/sizeof(*(a)))

static int ok, failed;

static int _icu_toASCII(const char *utf8, char **ascii, int transitional)
{
	UErrorCode status = 0;
	/*
	 * options
	 * UIDNA_ALLOW_UNASSIGNED (uts#46 disallows unassigned code points
	 * UIDNA_USE_STD3_RULES (restrict labels to LDH chars = ASCII letters, digits, hyphen-minus)
	 * UIDNA_CHECK_BIDI (error on BiDI code points)
	 * UIDNA_CHECK_CONTEXTJ (only relevant for compatibility of newer IDNA implementations with IDNA2003)
	 * UIDNA_CHECK_CONTEXTO  (for use by registries for IDNA2008 conformance, not requiered by UTS#46)
	 * UIDNA_NONTRANSITIONAL_TO_ASCII  (for nontransitional processing IDNA2008)
	 *
	 */
	UIDNA *idna = (void *)uidna_openUTS46(UIDNA_USE_STD3_RULES|UIDNA_CHECK_BIDI|(transitional?0:UIDNA_NONTRANSITIONAL_TO_ASCII), &status);
	int ret = -1;

	/* IDNA2008 UTS#46 punycode conversion */
	if (idna) {
		char lookupname[128] = "";
		UErrorCode status = 0;
		UIDNAInfo info = UIDNA_INFO_INITIALIZER;
		UChar utf16_dst[128], utf16_src[128];
		int32_t utf16_src_length;

		u_strFromUTF8(utf16_src, countof(utf16_src), &utf16_src_length, utf8, -1, &status);
		if (U_SUCCESS(status)) {
			// quick and dirty translation of '\uXXXX'
			int it, it2;
			for (it = 0, it2 = 0; it < utf16_src_length;) {
//				printf("char %c (%d)\n", utf16_src[it], utf16_src[it]);
				if (utf16_src[it] == '\\' && utf16_src[it + 1] == 'u') {
//					printf(" %2X %c\n", (utf16_src[it + 2] >= 'A' ? utf16_src[it + 2] - 'A' + 10 : utf16_src[it + 2] -'0'), utf16_src[it + 2]);
//					printf(" %2X %c\n", (utf16_src[it + 3] >= 'A' ? utf16_src[it + 3] - 'A' + 10 : utf16_src[it + 3] - '0'), utf16_src[it + 3]);
//					printf(" %2X %c\n", (utf16_src[it + 4] >= 'A' ? utf16_src[it + 4] - 'A' + 10 : utf16_src[it + 4] - '0'), utf16_src[it + 4]);
//					printf(" %2X %c\n", (utf16_src[it + 5] >= 'A' ? utf16_src[it + 5] - 'A' + 10 : utf16_src[it + 5] - '0'), utf16_src[it + 5]);
					utf16_src[it2++] =
						((utf16_src[it + 2] >= 'A' ? utf16_src[it + 2] - 'A' + 10 : utf16_src[it + 2] - '0') << 12) +
						((utf16_src[it + 3] >= 'A' ? utf16_src[it + 3] - 'A' + 10 : utf16_src[it + 3] - '0') << 8) +
						((utf16_src[it + 4] >= 'A' ? utf16_src[it + 4] - 'A' + 10 : utf16_src[it + 4] - '0') << 4) +
						(utf16_src[it + 5] >= 'A' ? utf16_src[it + 5] - 'A' + 10 : utf16_src[it + 5] - '0');
//					printf("new %04X\n", utf16_src[it2 - 1]);
					it += 6;
				} else
					utf16_src[it2++] = utf16_src[it++];
			}
//			printf("utf16_len %d -> %d\n", utf16_src_length, it2);
			utf16_src_length = it2;

			int32_t dst_length = uidna_nameToASCII((UIDNA *)idna, utf16_src, utf16_src_length, utf16_dst, countof(utf16_dst), &info, &status);
			if (U_SUCCESS(status)) {
				u_strToUTF8(lookupname, sizeof(lookupname), NULL, utf16_dst, dst_length, &status);
				if (U_SUCCESS(status)) {
					printf("transitionalDifferent: %d\n", (int) info.isTransitionalDifferent);
					if (ascii)
						if ((*ascii = strdup(lookupname)))
							ret = 0;
				} else
					printf("Failed to convert UTF-16 to UTF-8 (status %d)\n", status);
			} else
				printf("Failed to convert to ASCII (status %d)\n", status);
		} else
			printf("Failed to convert UTF-8 to UTF-16 (status %d)\n", status);

		uidna_close((UIDNA *)idna);
	}

	return ret;
}

static int _unistring_toASCII(const char *domain, char **ascii)
{
	const uint8_t *domain_u8 = (uint8_t *) domain;
	size_t len;
	uint32_t *domain_u32, *lower_u32;

	if (!(domain_u32 = u8_to_u32(domain_u8, u8_strlen(domain_u8) + 1, NULL, &len))) {
		printf("u8_to_u32(%s) failed (%d)\n", domain, errno);
		return -1;
	}
	printf("len1=%zu\n", len);

	lower_u32 = u32_tolower(domain_u32, len, 0, UNINORM_NFKC, NULL, &len);
	free(domain_u32);

	if (!lower_u32) {
		printf("u32_tolower(%s) failed (%d)\n", domain, errno);
		return -2;
	}
	printf("len2=%zu\n", len);

	if (ascii) {
		int rc;

		uint8_t *lower_u8 = u32_to_u8(lower_u32, len, NULL, &len);
		free(lower_u32);

		if (!lower_u8) {
			printf("u32_to_u8(%s) failed (%d)\n", domain, errno);
			return -3;
		}

		printf("lower_u8=%s\n", lower_u8);
		rc = idn2_lookup_u8(lower_u8, (uint8_t **) ascii, 0);
		free(lower_u8);

		if (rc != IDN2_OK) {
			printf("toASCII(%s) failed (%d): %s\n", domain, rc, idn2_strerror(rc));
			return -4;
		}
	} else {
		free(lower_u32);
	}

	return 0;
}

static void test_selected(void)
{
	static const struct test_data {
		const char
			*u8,
			*ace;
		int
			result;
	} test_data[] = {
		{ "straße.de", "xn--strae-oqa.de", 0 }, // different between IDNA 2003 and 2008
		{ "straẞe.de", "xn--strae-oqa.de", 0 }, // different between IDNA 2003 and 2008
	};
	char *ace = NULL;
	int n;

	for (unsigned it = 0; it < countof(test_data); it++) {
		const struct test_data *t = &test_data[it];

		n = _icu_toASCII(t->u8, &ace, 0);
		if (n != t->result) {
			failed++;
			printf("Failed [%u]: _icu_toASCII(%s) -> %d (expected %d)\n", it, t->u8, n, t->result);
		} else if (strcmp(t->ace, ace)) {
			failed++;
			printf("Failed [%u]: _icu_toASCII(%s) -> %s (expected %s)\n", it, t->u8, ace, t->ace);
		} else {
			ok++;
		}
		free(ace); ace = NULL;

		n = _unistring_toASCII(t->u8, &ace);
		if (n != t->result) {
			failed++;
			printf("Failed [%u]: _unistring_toASCII(%s) -> %d (expected %d)\n", it, t->u8, n, t->result);
		} else if (strcmp(t->ace, ace)) {
			failed++;
			printf("Failed [%u]: _unistring_toASCII(%s) -> %s (expected %s)\n", it, t->u8, ace, t->ace);
		} else {
			ok++;
		}
		free(ace); ace = NULL;
	}
}

static char *_nextField(char **line)
{
	char *s = *line, *e;

	if (!*s)
		return NULL;

	if (!(e = strpbrk(s, ";#"))) {
		e = *line += strlen(s);
	} else {
		*line = e + (*e == ';');
		*e = 0;
	}

	// trim leading and trailing whitespace
	while (isspace(*s)) s++;
	while (e > s && isspace(e[-1])) *--e = 0;

	return s;
}

static void test_IdnaTest(void)
{
	FILE *fp = fopen("IdnaTest.txt", "r");
	char *buf = NULL, *linep, *type, *source, *toUnicode, *toASCII, *NV8, *ace = NULL;
	size_t bufsize = 0;
	ssize_t buflen;
	int n;

	if (!fp) {
		fprintf(stderr, "Failed to open IdnaTest.txt (%d)\n", errno);
		return;
	}

	while ((buflen = getline(&buf, &bufsize, fp)) >= 0) {
		linep = buf;

		while (isspace(*linep)) linep++; // ignore leading whitespace
		if (!*linep || *linep == '#') continue; // skip empty lines and comments

		// strip off \r\n
		while (buflen > 0 && (buf[buflen] == '\n' || buf[buflen] == '\r'))
			buf[--buflen] = 0;

		type = _nextField(&linep);
		source  = _nextField(&linep);
		toUnicode  = _nextField(&linep);
		toASCII  = _nextField(&linep);
		NV8  = _nextField(&linep); // if set, the input should be disallowed for IDNA2008

		if (!*toUnicode)
			toUnicode = source;
		if (!*toASCII)
			toASCII = toUnicode;

		printf("##########%s#%s#%s#%s#%s#\n",type,source,toUnicode,toASCII,NV8);
		if (*type == 'B' || *type == 'T') {
			n = _icu_toASCII(source, &ace, 1);
			if (n) {
				failed++;
				printf("Failed: _icu_toASCII(%s) -> %d (expected 0)\n", source, n);
			} else if (strcmp(toASCII, ace)) {
				failed++;
				printf("Failed: _icu_toASCII(%s) -> %s (expected %s)\n", source, ace, toASCII);
			} else {
				printf("OK\n");
				ok++;
			}
			free(ace); ace = NULL;
		}
		if (*type == 'B' || *type == 'N') {
			n = _icu_toASCII(source, &ace, 0);
			if (n) {
				failed++;
				printf("Failed: _icu_toASCII(%s) -> %d (expected 0)\n", source, n);
			} else if (strcmp(toASCII, ace)) {
				failed++;
				printf("Failed: _icu_toASCII(%s) -> %s (expected %s)\n", source, ace, toASCII);
			} else {
				printf("OK\n");
				ok++;
			}
			free(ace); ace = NULL;
		}
	}

	free(buf);
	fclose(fp);
}

int main(int argc _U, const char **argv _U)
{
	test_IdnaTest(); // test all IDNA cases from Unicode 9.0.0
	test_selected(); // some manual selectioons

	if (failed) {
		printf("Summary: %d out of %d tests failed\n", failed, ok + failed);
		return 1;
	}

	printf("Summary: All %d tests passed\n", ok + failed);

	return 0;
}
