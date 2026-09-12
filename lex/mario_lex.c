#include "mario_lex.h"

#ifdef __cplusplus
extern "C" {
#endif

bool is_whitespace(unsigned char ch) {
	if(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
		return true;
	return false;
}

bool is_space(unsigned char ch) {
	if(ch == ' ' || ch == '\t' || ch == '\r')
		return true;
	return false;
}

bool is_numeric(unsigned char ch) {
	if(ch >= '0' && ch <= '9')
		return true;
	return false;
}

bool is_number(const char* cstr) {
	int  i= 0;
	while(cstr[i] != 0) {
		if (is_numeric(cstr[i]) == false)
			return false;
		i++;
	}
	return true;
}

bool is_hexadecimal(unsigned char ch) {
	if(((ch>='0') && (ch<='9')) ||
		((ch>='a') && (ch<='f')) ||
		((ch>='A') && (ch<='F')))
			return true;
	return false;
}

bool is_alpha(unsigned char ch) {
	if(((ch>='a') && (ch<='z')) ||
		((ch>='A') && (ch<='Z')) ||
		ch == '_')
		return true;
	return false;
}

bool is_alpha_num(const char* cstr) {
	if (cstr[0] == 0){
		return true;
	}
	if (is_alpha(cstr[0]) == 0){
		return false;
	}

	int  i= 0;
	while(cstr[i] != 0) {
		if (is_alpha(cstr[i]) == false || is_numeric(cstr[i]) == true){
			return false;
		}
		i++;
	}
	return true;
}

void lex_get_nextch(lex_t* lex) {
	lex->curr_ch = lex->next_ch;
	if (lex->data_pos < lex->data_end){
		lex->next_ch = lex->data[lex->data_pos];
	}else{
		lex->next_ch = 0;
	}
	lex->data_pos++;
}

void lex_skip_whitespace(lex_t* lex) {
	while (lex->curr_ch && is_whitespace(lex->curr_ch)){
		lex_get_nextch(lex);
	}
}

void lex_skip_space(lex_t* lex) {
	while (lex->curr_ch && is_space(lex->curr_ch)){
		lex_get_nextch(lex);
	}
}

//only take first 1~2 chars from start
bool lex_skip_comments_line(lex_t* lex, const char* start) {
	if(start[0] == 0)
		return false;

	if ((lex->curr_ch==start[0] && (start[1] == 0 || lex->next_ch==start[1]))) {
		while (lex->curr_ch && lex->curr_ch!='\n'){
			lex_get_nextch(lex);
		}
		if(start[1] != 0)
			lex_get_nextch(lex);
		return true;
	}
	return false;
}

//only take first 2 chars from start and end.
bool lex_skip_comments_block(lex_t* lex, const char* start, const char* end) {
	if(start[0] == 0 || start[1] == 0 || end[0] == 0 || end[1] == 0)
		return false;

	if (lex->curr_ch==start[0] && lex->next_ch==start[1]) {
		while (lex->curr_ch && (lex->curr_ch!=end[0] || lex->next_ch!=end[1])) {
			lex_get_nextch(lex);
		}
		lex_get_nextch(lex);
		lex_get_nextch(lex);
		return true;
	}	
	return false;
}

void lex_reset(lex_t* lex) {
	lex->data_pos = lex->data_start;
	lex->tk_start   = 0;
	lex->tk_end     = 0;
	lex->tk_last_end = 0;
	lex->tk  = LEX_EOF;
	mstr_reset(lex->tk_str);
	lex_get_nextch(lex);
	lex_get_nextch(lex);
}

void lex_init(lex_t * lex, const char* input) {
	lex->data = input;
	lex->data_start = 0;
	lex->data_end = (int)strlen(lex->data);
	lex->tk_str = mstr_new("");
	lex_reset(lex);
}

void lex_release(lex_t* lex) {
	mstr_free(lex->tk_str);
	lex->tk_str = NULL;
}

void lex_token_start(lex_t* lex) {
	// record beginning of this token(pre-read 2 chars );
	lex->tk_start = lex->data_pos-2;
}

void lex_token_end(lex_t* lex) {
	lex->tk_last_end = lex->tk_end;
	lex->tk_end = lex->data_pos-3;
}

void lex_get_char_token(lex_t* lex) {
	lex->tk = lex->curr_ch;
	if (lex->curr_ch) 
		lex_get_nextch(lex);
}

/* Value of a single hex digit, or -1 if not a hex digit. */
static int lex_hexval(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* Append the UTF-8 encoding of a Unicode code point to str (mario stores string
 * values as UTF-8). Astral code points (>0xFFFF) become 4 bytes. */
static void lex_add_codepoint(mstr_t* str, uint32_t cp) {
	if(cp < 0x80) {
		mstr_add(str, (char)cp);
	} else if(cp < 0x800) {
		mstr_add(str, (char)(0xC0 | (cp >> 6)));
		mstr_add(str, (char)(0x80 | (cp & 0x3F)));
	} else if(cp < 0x10000) {
		mstr_add(str, (char)(0xE0 | (cp >> 12)));
		mstr_add(str, (char)(0x80 | ((cp >> 6) & 0x3F)));
		mstr_add(str, (char)(0x80 | (cp & 0x3F)));
	} else {
		mstr_add(str, (char)(0xF0 | (cp >> 18)));
		mstr_add(str, (char)(0x80 | ((cp >> 12) & 0x3F)));
		mstr_add(str, (char)(0x80 | ((cp >> 6) & 0x3F)));
		mstr_add(str, (char)(0x80 | (cp & 0x3F)));
	}
}

/* Read a `\u` escape inside a string literal. On entry lex->curr_ch == 'u'.
 * Supports both `\uXXXX` (exactly 4 hex digits) and ES6 `\u{X...}` (braced code
 * point). A UTF-16 surrogate pair written as `\uD83D\uDE00` is combined into the
 * single astral code point U+1F600 so it is byte-identical to `\u{1F600}`.
 * On return lex->curr_ch is the LAST character consumed, matching the string
 * lexers' convention that the caller performs one trailing lex_get_nextch(). */
void lex_read_u_escape(lex_t* lex) {
	uint32_t cp = 0;
	lex_get_nextch(lex); /* step past 'u' */
	if(lex->curr_ch == '{') {
		lex_get_nextch(lex); /* first hex digit (or '}') */
		while(lex_hexval(lex->curr_ch) >= 0) {
			cp = cp * 16 + (uint32_t)lex_hexval(lex->curr_ch);
			lex_get_nextch(lex);
		}
		/* curr_ch is now '}' (first non-hex); leave it for the caller to skip. */
		if(cp > 0x10FFFF) cp = 0xFFFD; /* out-of-range -> replacement char */
		lex_add_codepoint(lex->tk_str, cp);
		return;
	}
	/* Exactly 4 hex digits; leave curr_ch on the 4th digit. */
	int n = 0;
	while(n < 4 && lex_hexval(lex->curr_ch) >= 0) {
		cp = cp * 16 + (uint32_t)lex_hexval(lex->curr_ch);
		n++;
		if(n < 4) lex_get_nextch(lex);
	}
	/* If this is a high surrogate, try to combine with a following \uDC00-\uDFFF. */
	if(cp >= 0xD800 && cp <= 0xDBFF) {
		int32_t sp = lex->data_pos; char sc = lex->curr_ch, sn = lex->next_ch;
		lex_get_nextch(lex); /* expect '\\' */
		if(lex->curr_ch == '\\') {
			lex_get_nextch(lex); /* expect 'u' */
			if(lex->curr_ch == 'u') {
				lex_get_nextch(lex); /* first hex digit of the low surrogate */
				uint32_t lo = 0; int m = 0;
				while(m < 4 && lex_hexval(lex->curr_ch) >= 0) {
					lo = lo * 16 + (uint32_t)lex_hexval(lex->curr_ch);
					m++;
					if(m < 4) lex_get_nextch(lex);
				}
				if(m == 4 && lo >= 0xDC00 && lo <= 0xDFFF) {
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					lex_add_codepoint(lex->tk_str, cp);
					return; /* curr_ch is the low surrogate's 4th digit */
				}
			}
		}
		/* Not a valid pair: rewind to just after the high surrogate. */
		lex->data_pos = sp; lex->curr_ch = sc; lex->next_ch = sn;
	}
	lex_add_codepoint(lex->tk_str, cp);
}

void lex_get_basic_token(lex_t* lex) {
	// tokens
	if (is_alpha(lex->curr_ch) || lex->curr_ch == '$') { //  IDs (JS allows '$' in identifiers, e.g. jQuery's $)
		while (is_alpha(lex->curr_ch) || is_numeric(lex->curr_ch) || lex->curr_ch == '$') {
			mstr_add(lex->tk_str, lex->curr_ch);
			lex_get_nextch(lex);
		}
		lex->tk = LEX_ID;
	} else if (is_numeric(lex->curr_ch) || (lex->curr_ch=='.' && is_numeric(lex->next_ch))) { // _numbers (incl. leading-dot floats like `.5`, common in minified JS)
		bool isHex = false;
		if (lex->curr_ch=='0') {
			mstr_add(lex->tk_str, lex->curr_ch);
			lex_get_nextch(lex);
		}
		if (lex->curr_ch=='x') {
			isHex = true;
			mstr_add(lex->tk_str, lex->curr_ch);
			lex_get_nextch(lex);
		}
		lex->tk = LEX_INT;

		while (is_numeric(lex->curr_ch) || (isHex && is_hexadecimal(lex->curr_ch)) || lex->curr_ch=='_') {
			// ES2021 numeric separator: '_' between digits is skipped.
			if (lex->curr_ch=='_') {
				if (is_numeric(lex->next_ch) || (isHex && is_hexadecimal(lex->next_ch))) {
					lex_get_nextch(lex);
					continue;
				}
				break;
			}
			mstr_add(lex->tk_str, lex->curr_ch);
			lex_get_nextch(lex);
		}
		if (!isHex && lex->curr_ch=='.' && (is_numeric(lex->next_ch) || lex->next_ch=='_')) {
			lex->tk = LEX_FLOAT;
			mstr_add(lex->tk_str, '.');
			lex_get_nextch(lex);
			while (is_numeric(lex->curr_ch) || lex->curr_ch=='_') {
				if (lex->curr_ch=='_') {
					if (is_numeric(lex->next_ch)) {
						lex_get_nextch(lex);
						continue;
					}
					break;
				}
				mstr_add(lex->tk_str, lex->curr_ch);
				lex_get_nextch(lex);
			}
		}
		// do fancy e-style floating point
		if (!isHex && (lex->curr_ch=='e'||lex->curr_ch=='E')) {
			lex->tk = LEX_FLOAT;
			mstr_add(lex->tk_str, lex->curr_ch);
			lex_get_nextch(lex);
			if (lex->curr_ch=='-') {
				mstr_add(lex->tk_str, lex->curr_ch);
				lex_get_nextch(lex);
			}
			while (is_numeric(lex->curr_ch)) {
				mstr_add(lex->tk_str, lex->curr_ch);
				lex_get_nextch(lex);
			}
		}
		// BigInt literal: a trailing 'n' on an integer literal (decimal or hex).
		// A float/exponent already set tk to LEX_FLOAT, so tk==LEX_INT excludes
		// them; the guard stops `1n` from absorbing a following identifier char.
		if (lex->tk == LEX_INT && lex->curr_ch=='n' &&
		    !is_alpha(lex->next_ch) && !is_numeric(lex->next_ch)) {
			lex->tk = LEX_BIGINT;
			lex_get_nextch(lex); // consume the 'n' suffix
		}
	} else if (lex->curr_ch=='"') {
		// strings...
		lex_get_nextch(lex);
		while (lex->curr_ch && lex->curr_ch!='"') {
			if (lex->curr_ch == '\\') {
				lex_get_nextch(lex);
				switch (lex->curr_ch) {
					case 'n' : mstr_add(lex->tk_str, '\n'); break;
					case 'r' : mstr_add(lex->tk_str, '\r'); break;
					case 't' : mstr_add(lex->tk_str, '\t'); break;
					case '"' : mstr_add(lex->tk_str, '\"'); break;
					case '\\' : mstr_add(lex->tk_str, '\\'); break;
					case 'u' : lex_read_u_escape(lex); break;
					default: mstr_add(lex->tk_str, lex->curr_ch);
				}
			} else {
				mstr_add(lex->tk_str, lex->curr_ch);
			}
			lex_get_nextch(lex);
		}
		lex_get_nextch(lex);
		lex->tk = LEX_STR;
	}
}

void lex_get_pos(lex_t* lex, int* line, int *col, int pos) {
	if (pos<0) 
		pos= lex->tk_last_end;

	int l = 1;
	int c  = 1;
	int i;
	for (i=0; i<pos; i++) {
		char ch;
		if (i < lex->data_end){
			ch = lex->data[i];
		}else{
			ch = 0;
		}

		c++;
		if (ch=='\n') {
			l++;
			c = 1;
		}
	}
	*line = l;
	*col = c;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

