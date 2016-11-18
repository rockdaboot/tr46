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
#include <unictype.h>

// includes for libidn2
#include <idn2.h>

#define _U __attribute__ ((unused))
#define countof(a) (sizeof(a)/sizeof(*(a)))

static int ok, failed;

#define IDNA_FLG_VALID                   1
#define IDNA_FLG_MAPPED                  2
#define IDNA_FLG_IGNORED                 4
#define IDNA_FLG_DEVIATION               8
#define IDNA_FLG_DISALLOWED             16
#define IDNA_FLG_DISALLOWED_STD3_MAPPED 32
#define IDNA_FLG_DISALLOWED_STD3_VALID  64

typedef struct {
	uint32_t
		cp1, cp2;
	uint32_t
		mapping[19];
	char
		flag;
} IDNAMap;

static IDNAMap idna_map[10000];
static size_t map_pos;

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

static int _scan_file(const char *fname, int(*scan)(char *))
{
	FILE *fp = fopen(fname, "r");
	char *buf = NULL, *linep;
	size_t bufsize = 0;
	ssize_t buflen;
	int ret = 0;

	if (!fp) {
		failed++;
		fprintf(stderr, "Failed to open IdnaTest.txt (%d)\n", errno);
		return -1;
	}

	while ((buflen = getline(&buf, &bufsize, fp)) >= 0) {
		linep = buf;

		while (isspace(*linep)) linep++; // ignore leading whitespace

		// strip off \r\n
		while (buflen > 0 && (buf[buflen] == '\n' || buf[buflen] == '\r'))
			buf[--buflen] = 0;

		if (!*linep || *linep == '#') continue; // skip empty lines and comments

		if ((ret = scan(linep))) {
			failed++;
			break;
		}
	}

	free(buf);
	fclose(fp);

	return ret;
}

static int read_IdnaMappings(char *linep)
{
	IDNAMap *map = &idna_map[map_pos];
	char *flag, *codepoint, *mapping;
	int n;

	codepoint = _nextField(&linep);
	flag      = _nextField(&linep);
	mapping   = _nextField(&linep);

	if ((n = sscanf(codepoint, "%X..%X", &map->cp1, &map->cp2)) == 1) {
		map->cp2 = map->cp1;
	} else if (n != 2) {
		printf("Failed to scan mapping codepoint '%s'\n", codepoint);
		return -1;
	}

	if (map->cp1 > map->cp2) {
		printf("Invalid codepoint range '%s'\n", codepoint);
		return -1;
	}

	if (map_pos && map->cp1 <= idna_map[map_pos - 1].cp2) {
		printf("Mapping codepoints out of order '%s'\n", codepoint);
		return -1;
	}

	if (!strcmp(flag, "valid"))
		map->flag = IDNA_FLG_VALID;
	else if (!strcmp(flag, "mapped"))
		map->flag = IDNA_FLG_MAPPED;
	else if (!strcmp(flag, "disallowed"))
		map->flag = IDNA_FLG_DISALLOWED;
	else if (!strcmp(flag, "ignored"))
		map->flag = IDNA_FLG_IGNORED;
	else if (!strcmp(flag, "deviation"))
		map->flag = IDNA_FLG_DEVIATION;
	else if (!strcmp(flag, "disallowed_STD3_mapped"))
		map->flag = IDNA_FLG_DISALLOWED_STD3_MAPPED;
	else if (!strcmp(flag, "disallowed_STD3_valid"))
		map->flag = IDNA_FLG_DISALLOWED_STD3_VALID;
	else {
		printf("Unknown flag '%s'\n", flag);
		return -1;
	}

	if (mapping && *mapping) {
		int n, pos;

		for (unsigned it = 0; it < countof(map->mapping); it++) {
			if ((n = sscanf(mapping, " %X%n", &map->mapping[it], &pos)) != 1)
				break;

			mapping += pos;
		}
		if (n == 1)
			printf("%s: Too many mappings '%s'\n", codepoint, mapping);
	} else if (map->flag == IDNA_FLG_MAPPED || map->flag == IDNA_FLG_DISALLOWED_STD3_MAPPED || map->flag == IDNA_FLG_DEVIATION) {
		if (map->cp1 != 0x200C && map->cp1 != 0x200D) // ZWNJ and ZWJ
			printf("Missing mapping for '%s'\n", codepoint);
	}

	if (++map_pos >= countof(idna_map)) {
		printf("Internal map size too small\n");
		return -1;
	}

	return 0;
}

static int _compare_map(IDNAMap *m1, IDNAMap *m2)
{
	if (m1->cp1 < m2->cp1)
		return -1;
	if (m1->cp1 > m2->cp2)
		return 1;
	return 0;
}

static IDNAMap *_get_map(uint32_t c)
{
	IDNAMap key;

	key.cp1 = c;
	return bsearch(&key, idna_map, map_pos, sizeof(IDNAMap), (int(*)(const void *, const void *))_compare_map);
}

static int32_t _icu_decodeIdnaTest(UChar *src, int32_t len)
{
	int it2 = 0;

	for (int it = 0; it < len;) {
		if (src[it] == '\\' && src[it + 1] == 'u') {
			src[it2++] =
				((src[it + 2] >= 'A' ? src[it + 2] - 'A' + 10 : src[it + 2] - '0') << 12) +
				((src[it + 3] >= 'A' ? src[it + 3] - 'A' + 10 : src[it + 3] - '0') << 8) +
				((src[it + 4] >= 'A' ? src[it + 4] - 'A' + 10 : src[it + 4] - '0') << 4) +
				(src[it + 5] >= 'A' ? src[it + 5] - 'A' + 10 : src[it + 5] - '0');
			it += 6;
		} else
			src[it2++] = src[it++];
	}

	return it2;
}

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
			utf16_src_length = _icu_decodeIdnaTest(utf16_src, utf16_src_length);

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

static size_t _unistring_decodeIdnaTest(uint32_t *src, size_t len)
{
	size_t it2 = 0;

	for (size_t it = 0; it < len;) {
		if (src[it] == '\\' && src[it + 1] == 'u') {
			src[it2++] =
				((src[it + 2] >= 'A' ? src[it + 2] - 'A' + 10 : src[it + 2] - '0') << 12) +
				((src[it + 3] >= 'A' ? src[it + 3] - 'A' + 10 : src[it + 3] - '0') << 8) +
				((src[it + 4] >= 'A' ? src[it + 4] - 'A' + 10 : src[it + 4] - '0') << 4) +
				(src[it + 5] >= 'A' ? src[it + 5] - 'A' + 10 : src[it + 5] - '0');
			it += 6;
		} else
			src[it2++] = src[it++];
	}

	return it2;
}

static int _check_label(uint32_t *label, size_t len)
{
	// 1. check for NFC
	// TODO

	// 2. The label must not contain a U+002D HYPHEN-MINUS character in both the third and fourth positions
	if (len >=4 && label[2] == '-' && label[3] == '-')
		return -1;

	// 3. The label must neither begin nor end with a U+002D HYPHEN-MINUS character
	if (len && (label[0] == '-' || label[len - 1] == '-'))
		return -1;

	// 4. The label must not contain a U+002E ( . ) FULL STOP
	for (size_t it = 0; it < len;)
		if (label[it] == '.')
			return -1;
	// 5. The label must not begin with a combining mark, that is: General_Category=Mark
	if (len && uc_combining_class(label[0]))
		return -1;

	// 6. Each code point in the label must only have certain status values according to Section 5, IDNA Mapping Table:
	//    a. For Transitional Processing, each value must be valid.
	//    b. For Nontransitional Processing, each value must be either valid or deviation.
	// TODO

	return 0;
}

/*
 * Processing of domain_name string as descripbed in
 *   http://www.unicode.org/reports/tr46/, 4 Processing
 */
static int _unistring_toASCII(const char *domain, char **ascii, int transitional)
{
	const uint8_t *domain_u8 = (uint8_t *) domain;
	size_t len;
	uint32_t *domain_u32;
	int err = 0;

	// convert UTF-8 to UCS-4 (Unicode))
	if (!(domain_u32 = u8_to_u32(domain_u8, u8_strlen(domain_u8) + 1, NULL, &len))) {
		printf("u8_to_u32(%s) failed (%d)\n", domain, errno);
		return -1;
	}
	printf("len1=%zu\n", len);

	// quick and dirty translation of '\uXXXX' found in IdnaTest.txt
	len = _unistring_decodeIdnaTest(domain_u32, len);

	size_t len2 = 0;
	for (size_t it = 0; it < len; it++) {
		IDNAMap *map = _get_map(domain_u32[it]);

		if (!map || map->flag == IDNA_FLG_DISALLOWED) {
			len2++;
			err = 1;
		} else if (map->flag == IDNA_FLG_MAPPED) {
			for (int it2 = 0; map->mapping[it2];)
				len2++;
		} else if (map->flag == IDNA_FLG_VALID) {
			len2++;
		} else if (map->flag == IDNA_FLG_IGNORED) {
			continue;
		} else if (map->flag == IDNA_FLG_DEVIATION) {
			if (transitional)
				for (int it2 = 0; map->mapping[it2];)
					len2++;
		}
	}

	uint32_t *tmp = malloc(len2 * sizeof(uint32_t));

	for (size_t it = 0; it < len; it++) {
		uint32_t c = domain_u32[it];
		IDNAMap *map = _get_map(c);

		if (!map || map->flag == IDNA_FLG_DISALLOWED) {
			tmp[len2++] = c;
			err = 1;
		} else if (map->flag == IDNA_FLG_MAPPED) {
			for (int it2 = 0; map->mapping[it2];)
				tmp[len2++] = map->mapping[it2];
		} else if (map->flag == IDNA_FLG_VALID) {
			tmp[len2++] = c;
		} else if (map->flag == IDNA_FLG_IGNORED) {
			continue;
		} else if (map->flag == IDNA_FLG_DEVIATION) {
			if (transitional)
				for (int it2 = 0; map->mapping[it2];)
					tmp[len2++] = map->mapping[it2];
		}
	}
	free(domain_u32);

	// Normalize to NFC
	domain_u32 = u32_normalize(UNINORM_NFC, tmp, len2, NULL, &len);
	free(tmp); tmp = NULL;

	if (!domain_u32) {
		printf("u32_normalize(%s) failed (%d)\n", domain, errno);
		return -2;
	}
	printf("len=%zu\n", len);

	// split into labels and check
	uint32_t *e, *s;
	e = s = domain_u32;
	for (e = s = domain_u32; *e; s = ++e) {
		while (*e && *e != '.') e++;
		if (_check_label(s, e - s))
			err = 1;
	}

	if (ascii) {
		int rc;

		uint8_t *lower_u8 = u32_to_u8(domain_u32, len, NULL, &len);
		free(domain_u32);

		if (!lower_u8) {
			printf("u32_to_u8(%s) failed (%d)\n", domain, errno);
			return -3;
		}

		printf("lower_u8=%s\n", lower_u8);
		rc = idn2_lookup_u8(lower_u8, (uint8_t **) ascii, 0);
		free(lower_u8);

		if (rc != IDN2_OK) {
			*ascii = NULL; /* libidn2 taints the variable on error */
			printf("toASCII(%s) failed (%d): %s\n", domain, rc, idn2_strerror(rc));
			return -4;
		}
	} else {
		free(domain_u32);
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

		n = _unistring_toASCII(t->u8, &ace, 0);
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

static void _check_toASCII(char *source, char *expected, int transitional)
{
	int n;
	char *ace = NULL;

	n = _icu_toASCII(source, &ace, transitional);

	if (n) {
		failed++;
		printf("Failed: _icu_toASCII(%s) -> %d (expected 0)\n", source, n);
	} else if (strcmp(expected, ace)) {
		failed++;
		printf("Failed: _icu_toASCII(%s) -> %s (expected %s)\n", source, ace, expected);
	} else {
		printf("OK\n");
		ok++;
	}

	free(ace); ace = NULL;

	n = _unistring_toASCII(source, &ace, transitional);

	if (n) {
		failed++;
		printf("Failed: _unistring_toASCII(%s) -> %d (expected 0) %p\n", source, n, ace);
	} else if (strcmp(expected, ace)) {
		failed++;
		printf("Failed: _unistring_toASCII(%s) -> %s (expected %s) %p\n", source, ace, expected, ace);
	} else {
		printf("OK\n");
		ok++;
	}

	free(ace); ace = NULL;
}

static int test_IdnaTest(char *linep)
{
	char *type, *source, *toUnicode, *toASCII, *NV8;

	type = _nextField(&linep);
	source  = _nextField(&linep);
	toUnicode  = _nextField(&linep);
	toASCII  = _nextField(&linep);
	NV8  = _nextField(&linep); // if set, the input should be disallowed for IDNA2008

	if (!*toUnicode)
		toUnicode = source;
	if (!*toASCII)
		toASCII = toUnicode;

	printf("##########%s#%s#%s#%s#%s#\n", type, source, toUnicode, toASCII, NV8);
	if (*type == 'B') {
		_check_toASCII(source, toASCII, 1);
		_check_toASCII(source, toASCII, 0);
	} else if (*type == 'T') {
		_check_toASCII(source, toASCII, 1);
	} else if (*type == 'N') {
		_check_toASCII(source, toASCII, 0);
	} else {
		printf("Failed: Unknown type '%s'\n", type);
	}

	return 0;
}

int main(int argc _U, const char **argv _U)
{
	// read IDNA mappings
	if (_scan_file("IdnaMappingTable.txt", read_IdnaMappings))
		goto out;

	// test all IDNA cases from Unicode 9.0.0
	if (_scan_file("IdnaTest.txt", test_IdnaTest))
		goto out;

	test_selected(); // some manual selections

out:
	if (failed) {
		printf("Summary: %d out of %d tests failed\n", failed, ok + failed);
		return 1;
	}

	printf("Summary: All %d tests passed\n", ok + failed);

	return 0;
}