#include "mario.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**======platform porting functions======*/

void* (*_platform_malloc)(uint32_t size) = NULL;
void  (*_platform_free)(void* p) = NULL;

void  (*_platform_out)(const char*) = NULL;

inline void* mario_malloc(uint32_t size) {
	return _platform_malloc(size);
}

inline void mario_free(void* p) {
	if(p != NULL)
		_platform_free(p);
}

void  free_none(void* p) { }

void *mario_realloc(void* p, uint32_t old_size, uint32_t new_size) {
	void *np = mario_malloc(new_size);
	if(p != NULL) {
		memcpy(np, p, old_size);
		mario_free(p);
	}
	return np;
}

/**======debug functions======*/
static inline void dout(const char* s) {
	if(_platform_out != NULL)
		_platform_out(s);
}

#define BUF_SIZE 512

inline void mario_debug(const char *format, ...) {
#if MARIO_DEBUG
	char buf[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, BUF_SIZE, format, ap);
	va_end(ap);
	dout(buf);
#endif
}

inline void mario_printf(const char *format, ...) {
	char buf[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, BUF_SIZE, format, ap);
	va_end(ap);
	dout(buf);
}

/**======hash map functions======*/

#define HASH_MAP_INITIAL_CAPACITY 16

// Hash function for strings
static uint32_t hash_string(const char* key) { 
    uint32_t hash = 0; 
    while (*key) { 
        hash = (hash << 5) - hash + *key; 
        key++; 
    } 
    return hash; 
}

// Create a new hash map
hash_map_t* hash_map_new(void) {
    hash_map_t* map = (hash_map_t*)mario_malloc(sizeof(hash_map_t));
    hash_map_init(map);
    return map;
}

// Initialize a hash map
void hash_map_init(hash_map_t* map) {
    map->capacity = HASH_MAP_INITIAL_CAPACITY;
    map->size = 0;
    map->load_factor_num = 3;   // 分子：3
    map->load_factor_den = 4;   // 分母：4（3/4 = 0.75）
    map->buckets = (hash_entry_t**)mario_malloc(sizeof(hash_entry_t*) * map->capacity);
    uint32_t i;
    for (i = 0; i < map->capacity; i++) {
        map->buckets[i] = NULL;
    }
}

// Add a key-value pair to the hash map
void hash_map_add(hash_map_t* map, const char* key, void* value) {
    // Check if we need to resize
    if (map->size * map->load_factor_den > map->capacity * map->load_factor_num) {
        // Resize the hash map
        uint32_t new_capacity = map->capacity * 2;
        hash_entry_t** new_buckets = (hash_entry_t**)mario_malloc(sizeof(hash_entry_t*) * new_capacity);
        uint32_t i;
        for (i = 0; i < new_capacity; i++) {
            new_buckets[i] = NULL;
        }
        
        // Rehash all entries
        for (i = 0; i < map->capacity; i++) {
            hash_entry_t* entry = map->buckets[i];
            while (entry) {
                hash_entry_t* next = entry->next;
                uint32_t hash = hash_string(entry->key) % new_capacity;
                entry->next = new_buckets[hash];
                new_buckets[hash] = entry;
                entry = next;
            }
        }
        
        // Free old buckets
        mario_free(map->buckets);
        map->buckets = new_buckets;
        map->capacity = new_capacity;
    }
    
    // Calculate hash
    uint32_t hash = hash_string(key) % map->capacity;
    
    // Check if key already exists
    hash_entry_t* entry = map->buckets[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Key exists, update value
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Key doesn't exist, create new entry
    entry = (hash_entry_t*)mario_malloc(sizeof(hash_entry_t));
    entry->key = (char*)mario_malloc(strlen(key) + 1);
    strcpy(entry->key, key);
    entry->value = value;
    entry->next = map->buckets[hash];
    map->buckets[hash] = entry;
    map->size++;
}

// Get a value from the hash map
void* hash_map_get(hash_map_t* map, const char* key) {
    uint32_t hash = hash_string(key) % map->capacity;
    hash_entry_t* entry = map->buckets[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

// Remove a key-value pair from the hash map
void* hash_map_remove(hash_map_t* map, const char* key) {
    uint32_t hash = hash_string(key) % map->capacity;
    hash_entry_t* entry = map->buckets[hash];
    hash_entry_t* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Found the entry
            void* value = entry->value;
            
            // Remove from bucket
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[hash] = entry->next;
            }
            
            // Free entry
            mario_free(entry->key);
            mario_free(entry);
            map->size--;
            
            return value;
        }
        prev = entry;
        entry = entry->next;
    }
    
    return NULL;
}

// Get the size of the hash map
uint32_t hash_map_size(hash_map_t* map) {
    return map->size;
}

// Clean the hash map
void hash_map_clean(hash_map_t* map, free_func_t free_key, free_func_t free_value) {
    uint32_t i;
    for (i = 0; i < map->capacity; i++) {
        hash_entry_t* entry = map->buckets[i];
        while (entry) {
            hash_entry_t* next = entry->next;
            if (free_key) {
                free_key(entry->key);
            }
            if (free_value && entry->value != NULL) {
                free_value(entry->value);
            }
            mario_free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
	mario_free(map->buckets);
	map->buckets = NULL;
    map->size = 0;
}

// Free the hash map
void hash_map_free(hash_map_t* map, free_func_t free_key, free_func_t free_value) {
    hash_map_clean(map, free_key, free_value);
    mario_free(map);
}

// Iterate over the hash map
void hash_map_iterate(hash_map_t* map, void (*callback)(const char* key, void* value, void* user_data), void* user_data) {
    uint32_t i;
    for (i = 0; i < map->capacity; i++) {
        hash_entry_t* entry = map->buckets[i];
        while (entry) {
            callback(entry->key, entry->value, user_data);
            entry = entry->next;
        }
    }
}

/**======array functions======*/

#define ARRAY_BUF 16

inline void array_init(m_array_t* array) { 
	array->items = NULL; 
	array->size = 0; 
	array->max = 0; 
}

m_array_t* array_new() {
	m_array_t* ret = (m_array_t*)mario_malloc(sizeof(m_array_t));
	array_init(ret);
	return ret;
}

inline void array_add(m_array_t* array, void* item) {
	uint32_t new_size = array->size + 1; 
	if(array->max <= new_size) { 
		new_size = array->size + ARRAY_BUF;
		array->items = (void**)mario_realloc(array->items, array->max*sizeof(void*), new_size*sizeof(void*)); 
		array->max = new_size; 
	} 
	array->items[array->size] = item; 
	array->size++; 
	array->items[array->size] = NULL; 
}

inline void array_add_head(m_array_t* array, void* item) {
	uint32_t new_size = array->size + 1; 
	if(array->max <= new_size) { 
		new_size = array->size + ARRAY_BUF;
		array->items = (void**)mario_realloc(array->items, array->max*sizeof(void*), new_size*sizeof(void*)); 
		array->max = new_size; 
	} 
	int32_t i;
	for(i=array->size; i>0; i--) {
		array->items[i] = array->items[i-1];
	}
	array->items[0] = item; 
	array->size++; 
	array->items[array->size] = NULL; 
}

void* array_add_buf(m_array_t* array, void* s, uint32_t sz) {
	void* item = mario_malloc(sz);
	if(s != NULL)
		memcpy(item, s, sz);
	array_add(array, item);
	return item;
}

inline void* array_get(m_array_t* array, uint32_t index) {
	if(array->items == NULL || index >= array->size)
		return NULL;
	return array->items[index];
}

inline void* array_set(m_array_t* array, uint32_t index, void* p) {
	if(array->items == NULL || index >= array->size)
		return NULL;
	array->items[index] = p;
	return p;
}

inline void* array_head(m_array_t* array) {
	if(array->items == NULL || array->size == 0)
		return NULL;
	return array->items[0];
}

inline void* array_remove(m_array_t* array, uint32_t index) { //remove out but not free
	if(index >= array->size)
		return NULL;

	void *p = array->items[index];
	uint32_t i;
	for(i=index; (i+1)<array->size; i++) {
		array->items[i] = array->items[i+1];	
	}

	array->size--;
	array->items[array->size] = NULL;
	return p;
}

inline void array_del(m_array_t* array, uint32_t index, free_func_t fr) { // remove out and free.
	void* p = array_remove(array, index);
	if(p != NULL) {
		if(fr != NULL)
			fr(p);
		else
			mario_free(p);
	}
}

inline void array_remove_all(m_array_t* array) { //remove all items bot not free them.
	if(array->items != NULL) {
		mario_free(array->items);
		array->items = NULL;
	}
	array->max = array->size = 0;
}

inline void array_free(m_array_t* array, free_func_t fr) {
	array_clean(array, fr);
	mario_free(array);
}

inline void array_clean(m_array_t* array, free_func_t fr) { //remove all items and free them.
	if(array->items != NULL) {
		uint32_t i;
		for(i=0; i<array->size; i++) {
			void* p = array->items[i];
			if(p != NULL) {
				if(fr != NULL)
					fr(p);
				else
					mario_free(p);
			}
		}
		mario_free(array->items);
		array->items = NULL;
	}
	array->max = array->size = 0;
}

/**======string functions======*/

#define mstr_BUF 16

void mstr_reset(mstr_t* str) {
	if(str->cstr == NULL) {
		str->cstr = (char*)mario_malloc(mstr_BUF);
		str->max = mstr_BUF;
	}

	str->cstr[0] = 0;
	str->len = 0;	
}

char* mstr_ncpy(mstr_t* str, const char* src, uint32_t l) {
	if(src == NULL || src[0] == 0 || l == 0) {
		mstr_reset(str);
		return str->cstr;
	}

	uint32_t len = (uint32_t)strlen(src);
	if(len > l)
		len = l;

	uint32_t new_size = len;
	if(str->max <= new_size) {
		new_size = len + mstr_BUF; /*STR BUF for buffer*/
		str->cstr = (char*)mario_realloc(str->cstr, str->max, new_size);
		str->max = new_size;
	}

	strncpy(str->cstr, src, len);
	str->cstr[len] = 0;
	str->len = len;
	return str->cstr;
}

char* mstr_cpy(mstr_t* str, const char* src) {
	mstr_ncpy(str, src, 0x0FFFF);
	return str->cstr;
}

mstr_t* mstr_new(const char* s) {
	mstr_t* ret = (mstr_t*)mario_malloc(sizeof(mstr_t));
	ret->cstr = NULL;
	ret->max = 0;
	ret->len = 0;
	mstr_cpy(ret, s);
	return ret;
}

mstr_t* mstr_new_by_size(uint32_t sz) {
	mstr_t* ret = (mstr_t*)mario_malloc(sizeof(mstr_t));
	ret->cstr = (char*)mario_malloc(sz);
	ret->max = sz;
	ret->cstr[0] = 0;
	ret->len = 0;
	return ret;
}

char* mstr_append(mstr_t* str, const char* src) {
	if(src == NULL || src[0] == 0) {
		return str->cstr;
	}

	uint32_t len = (uint32_t)strlen(src);
	uint32_t new_size = str->len + len;
	if(str->max <= new_size) {
		new_size = str->len + len + mstr_BUF; /*STR BUF for buffer*/
		str->cstr = (char*)mario_realloc(str->cstr, str->max, new_size);
		str->max = new_size;
	}

	strcpy(str->cstr + str->len, src);
	str->len = str->len + len;
	str->cstr[str->len] = 0;
	return str->cstr;
}

char* mstr_add(mstr_t* str, char c) {
	uint32_t new_size = str->len + 1;
	if(str->max <= new_size) {
		new_size = str->len + mstr_BUF; /*STR BUF for buffer*/
		str->cstr = (char*)mario_realloc(str->cstr, str->max, new_size);
		str->max = new_size;
	}

	str->cstr[str->len] = c;
	str->len++;
	str->cstr[str->len] = 0;
	return str->cstr;
}

char* mstr_add_int(mstr_t* str, int i, int base) {
	return mstr_append(str, mstr_from_int(i, base));
}

char* mstr_add_float(mstr_t* str, float f) {
	return mstr_append(str, mstr_from_float(f));
}

void mstr_free(mstr_t* str) {
	if(str == NULL)
		return;

	if(str->cstr != NULL) {
		mario_free(str->cstr);
	}
	mario_free(str);
}

static char _mstr_result[STATIC_mstr_MAX+1];

const char* mstr_from_int(int value, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) 
			base = 10;

    char* ptr = _mstr_result, *ptr1 = _mstr_result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return _mstr_result;
}

const char* mstr_from_bool(bool b) {
	return b ? "true":"false";
}

const char* mstr_from_float(float i) {
	/* Match JS Number->String: NaN/Infinity render as their keyword forms and a
	 * finite value drops the zeros "%f" pads (3.500000 -> 3.5, 2.000000 -> 2).
	 * Keeping "%f" (not "%g") preserves plain decimal notation for the magnitudes
	 * a 32-bit float is normally used for, so no surprising 1e+06 style output. */
	if(isnan(i)) {
		snprintf(_mstr_result, STATIC_mstr_MAX-1, "NaN");
		return _mstr_result;
	}
	if(isinf(i)) {
		snprintf(_mstr_result, STATIC_mstr_MAX-1, i < 0 ? "-Infinity" : "Infinity");
		return _mstr_result;
	}
	snprintf(_mstr_result, STATIC_mstr_MAX-1, "%f", i);
	char* dot = strchr(_mstr_result, '.');
	if(dot != NULL) {
		char* end = _mstr_result + strlen(_mstr_result) - 1;
		while(end > dot && *end == '0')
			*end-- = '\0';
		if(end == dot) //all decimals were zero: drop the dangling point too
			*end = '\0';
	}
	return _mstr_result;
}

const char* mstr_from_int64(int64_t value, int base) {
	// check that the base is valid
	if (base < 2 || base > 36)
		base = 10;

	char* ptr = _mstr_result, *ptr1 = _mstr_result, tmp_char;
	/* Accumulate as unsigned so LLONG_MIN (which has no positive int64
	 * counterpart) formats correctly without signed-overflow UB. */
	uint64_t uvalue;
	bool neg = false;
	if (value < 0) {
		neg = true;
		uvalue = (uint64_t)(-(value + 1)) + 1;
	} else {
		uvalue = (uint64_t)value;
	}

	int tmp_digit;
	do {
		tmp_digit = (int)(uvalue % (uint64_t)base);
		uvalue /= (uint64_t)base;
		*ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_digit];
	} while ( uvalue );

	// Apply negative sign
	if (neg) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return _mstr_result;
}

const char* mstr_from_float64(double d) {
	/* JS Number::toString(radix 10): NaN/Infinity render as their keyword
	 * forms; a finite value uses the shortest decimal digit sequence that
	 * round-trips back to the same double, laid out in plain notation inside
	 * [1e-6, 1e21) and exponential notation outside it. */
	if (isnan(d)) {
		snprintf(_mstr_result, STATIC_mstr_MAX, "NaN");
		return _mstr_result;
	}
	if (isinf(d)) {
		snprintf(_mstr_result, STATIC_mstr_MAX, d < 0 ? "-Infinity" : "Infinity");
		return _mstr_result;
	}

	char* out = _mstr_result;
	int oi = 0;
	if (d < 0) { out[oi++] = '-'; d = -d; }
	if (d == 0.0) { out[oi++] = '0'; out[oi] = 0; return _mstr_result; }

	/* Find the minimal precision p (0..17 significant-1 digits) whose
	 * scientific rendering parses back to exactly d. %.17e always round-trips
	 * for any double, so the loop terminates by p == 17 at the latest. */
	char buf[64];
	int p;
	for (p = 0; p <= 17; p++) {
		snprintf(buf, sizeof(buf), "%.*e", p, d);
		if (strtod(buf, NULL) == d)
			break;
	}

	/* Split "D[.DDDD]e±EE" into its significant digits and base-10 exponent. */
	char* epos = strchr(buf, 'e');
	int exp10 = epos ? atoi(epos + 1) : 0;
	if (epos) *epos = 0;
	char digits[32];
	int di = 0;
	for (char* c = buf; *c; c++) {
		if (*c == '.') continue;
		digits[di++] = *c;
	}
	digits[di] = 0;
	int k = di;          // number of significant digits (>= 1)
	int n = exp10 + 1;   // decimal-point position relative to digits[0]

	if (k <= n && n <= 21) {
		// Integer with trailing zeros: 1e21 -> "1000000000000000000000"
		for (int i = 0; i < k; i++) out[oi++] = digits[i];
		for (int i = 0; i < n - k; i++) out[oi++] = '0';
	} else if (0 < n && n <= 21) {
		// Decimal point sits inside the digits: 1.5 -> "1.5"
		for (int i = 0; i < n; i++) out[oi++] = digits[i];
		out[oi++] = '.';
		for (int i = n; i < k; i++) out[oi++] = digits[i];
	} else if (-6 < n && n <= 0) {
		// Small magnitude: 1e-6 -> "0.000001"
		out[oi++] = '0'; out[oi++] = '.';
		for (int i = 0; i < -n; i++) out[oi++] = '0';
		for (int i = 0; i < k; i++) out[oi++] = digits[i];
	} else {
		// Exponential form: 1e21 -> "1e+21", 1.5e22 -> "1.5e+22"
		out[oi++] = digits[0];
		if (k > 1) {
			out[oi++] = '.';
			for (int i = 1; i < k; i++) out[oi++] = digits[i];
		}
		out[oi++] = 'e';
		int e = n - 1;
		if (e < 0) { out[oi++] = '-'; e = -e; }
		else out[oi++] = '+';
		oi += snprintf(out + oi, STATIC_mstr_MAX - oi, "%d", e);
	}
	out[oi] = 0;
	return _mstr_result;
}

int mstr_to_int(const char* str) {
	int i = 0;
	if(strstr(str, "0x") != NULL ||
			strstr(str, "0x") != NULL)
		i = (int)strtol(str, NULL, 16);
	else
		i = (int)strtol(str, NULL, 10);
	return i;
}

float mstr_to_float(const char* str) {
	return (float)strtod(str, NULL);
}

void mstr_split(const char* str, char c, m_array_t* array) {
	int i = 0;
	char offc = str[i];
	while(true) {
		if(offc == c || offc == 0) {
			char* p = (char*)mario_malloc(i+1);
			memcpy(p, str, i+1);
			p[i] = 0;
			array_add(array, p);
			if(offc == 0)
				break;

			str = str +  i + 1;
			i = 0;
			offc = str[i]; 
		}
		else {
			i++;
			offc = str[i]; 
		}
	}
}

int mstr_to(const char* str, char c, mstr_t* res, bool skipspace) {
	int i = 0;
	mstr_reset(res);

	while(true) {
		char offc = str[i]; 
		if(offc==0) {//the end of str
			return -1;
		}

		//skip space
		if(skipspace && (offc == ' ' || offc == '\t')) {
			i++;
			continue;
		}

		if(offc == c) 
			break;
		else
			mstr_add(res, offc);
		i++;
	}

	if(skipspace) {
		int j = res->len - 1;
		while(j >= 0) {
			char c = res->cstr[j];
			if(c == ' ' || c == '\t')
				res->cstr[j] = 0;
			j--;
		}
	}

	return i;
}

/**======bignum (ES2020 BigInt) library======*/

/* Sign-magnitude arbitrary precision integer. `limbs` is a little-endian
 * base-2^32 magnitude (limbs[0] least significant) with no redundant leading
 * zero limb (bn_normalize keeps that invariant); `sign` is -1/0/+1 and is 0
 * exactly when the value is zero. All public bn_* functions return fresh
 * bignum_t* owned by the caller (freed with bn_free), except the *_inplace
 * helpers. Allocation failures are not modelled: mario_malloc aborts. */

static inline void bn_normalize(bignum_t* b) {
	while(b->len > 0 && b->limbs[b->len-1] == 0)
		b->len--;
	if(b->len == 0)
		b->sign = 0;
}

static inline void bn_grow(bignum_t* b, uint32_t n) {
	if(n <= b->cap)
		return;
	uint32_t cap = b->cap ? b->cap : 2;
	while(cap < n)
		cap *= 2;
	uint32_t* p = (uint32_t*)mario_malloc(cap * sizeof(uint32_t));
	if(b->limbs != NULL) {
		memcpy(p, b->limbs, b->cap * sizeof(uint32_t));
		mario_free(b->limbs);
	}
	for(uint32_t i = b->cap; i < cap; i++)
		p[i] = 0;
	b->limbs = p;
	b->cap = cap;
}

bignum_t* bn_new(void) {
	bignum_t* b = (bignum_t*)mario_malloc(sizeof(bignum_t));
	b->sign = 0;
	b->len = 0;
	b->cap = 0;
	b->limbs = NULL;
	return b;
}

void bn_free(void* p) {
	if(p == NULL)
		return;
	bignum_t* b = (bignum_t*)p;
	if(b->limbs != NULL)
		mario_free(b->limbs);
	mario_free(b);
}

bignum_t* bn_clone(const bignum_t* src) {
	bignum_t* b = bn_new();
	if(src->len > 0) {
		bn_grow(b, src->len);
		memcpy(b->limbs, src->limbs, src->len * sizeof(uint32_t));
		b->len = src->len;
		b->sign = src->sign;
	}
	return b;
}

bool bn_is_zero(const bignum_t* b) {
	return b == NULL || b->len == 0 || b->sign == 0;
}

bignum_t* bn_from_uint64(uint64_t v) {
	bignum_t* b = bn_new();
	if(v == 0)
		return b;
	bn_grow(b, 2);
	b->limbs[0] = (uint32_t)(v & 0xFFFFFFFFULL);
	b->limbs[1] = (uint32_t)(v >> 32);
	b->len = (b->limbs[1] != 0) ? 2 : 1;
	b->sign = 1;
	return b;
}

bignum_t* bn_from_int64(int64_t v) {
	uint64_t m = (v < 0) ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
	bignum_t* b = bn_from_uint64(m);
	if(v < 0 && b->len > 0)
		b->sign = -1;
	return b;
}

/* Compare magnitudes (ignore sign). */
static int bn_cmp_abs(const bignum_t* a, const bignum_t* b) {
	if(a->len != b->len)
		return (a->len > b->len) ? 1 : -1;
	for(uint32_t i = a->len; i > 0; i--) {
		uint32_t x = a->limbs[i-1], y = b->limbs[i-1];
		if(x != y)
			return (x > y) ? 1 : -1;
	}
	return 0;
}

int bn_cmp(const bignum_t* a, const bignum_t* b) {
	if(a->sign != b->sign)
		return (a->sign > b->sign) ? 1 : -1;
	if(a->sign == 0)
		return 0;
	int c = bn_cmp_abs(a, b);
	return (a->sign > 0) ? c : -c;
}

/* r = |a| + |b| (sign left at 0 for the caller to set). */
static bignum_t* bn_add_abs(const bignum_t* a, const bignum_t* b) {
	bignum_t* r = bn_new();
	uint32_t n = (a->len > b->len) ? a->len : b->len;
	bn_grow(r, n + 1);
	uint64_t carry = 0;
	for(uint32_t i = 0; i < n; i++) {
		uint64_t av = (i < a->len) ? a->limbs[i] : 0;
		uint64_t bv = (i < b->len) ? b->limbs[i] : 0;
		uint64_t s = av + bv + carry;
		r->limbs[i] = (uint32_t)(s & 0xFFFFFFFFULL);
		carry = s >> 32;
	}
	if(carry)
		r->limbs[n++] = (uint32_t)carry;
	r->len = n;
	bn_normalize(r);
	return r;
}

/* r = |a| - |b| (magnitudes); precondition |a| >= |b|. */
static bignum_t* bn_sub_abs(const bignum_t* a, const bignum_t* b) {
	bignum_t* r = bn_new();
	uint32_t n = a->len;
	bn_grow(r, n ? n : 1);
	int64_t borrow = 0;
	for(uint32_t i = 0; i < n; i++) {
		int64_t av = (int64_t)a->limbs[i];
		int64_t bv = (i < b->len) ? (int64_t)b->limbs[i] : 0;
		int64_t d = av - bv - borrow;
		if(d < 0) { d += 4294967296LL; borrow = 1; }
		else borrow = 0;
		r->limbs[i] = (uint32_t)d;
	}
	r->len = n;
	bn_normalize(r);
	return r;
}

/* r -= |b| in place (magnitudes); precondition |r| >= |b|. */
static void bn_sub_abs_inplace(bignum_t* r, const bignum_t* b) {
	int64_t borrow = 0;
	for(uint32_t i = 0; i < r->len; i++) {
		int64_t rv = (int64_t)r->limbs[i];
		int64_t bv = (i < b->len) ? (int64_t)b->limbs[i] : 0;
		int64_t d = rv - bv - borrow;
		if(d < 0) { d += 4294967296LL; borrow = 1; }
		else borrow = 0;
		r->limbs[i] = (uint32_t)d;
	}
	bn_normalize(r);
}

bignum_t* bn_add(const bignum_t* a, const bignum_t* b) {
	if(a->sign == 0) return bn_clone(b);
	if(b->sign == 0) return bn_clone(a);
	if(a->sign == b->sign) {
		bignum_t* r = bn_add_abs(a, b);
		r->sign = a->sign;
		return r;
	}
	int c = bn_cmp_abs(a, b);
	if(c == 0) return bn_new();
	if(c > 0) {
		bignum_t* r = bn_sub_abs(a, b);
		r->sign = a->sign;
		return r;
	}
	bignum_t* r = bn_sub_abs(b, a);
	r->sign = b->sign;
	return r;
}

bignum_t* bn_sub(const bignum_t* a, const bignum_t* b) {
	bignum_t nb;              // -b without allocating: bn_add only reads it
	nb.sign = -b->sign;
	nb.len = b->len;
	nb.cap = b->cap;
	nb.limbs = b->limbs;
	return bn_add(a, &nb);
}

bignum_t* bn_neg(const bignum_t* a) {
	bignum_t* r = bn_clone(a);
	r->sign = -a->sign;
	return r;
}

bignum_t* bn_mul(const bignum_t* a, const bignum_t* b) {
	if(a->sign == 0 || b->sign == 0)
		return bn_new();
	uint32_t n = a->len + b->len;
	bignum_t* r = bn_new();
	bn_grow(r, n);
	for(uint32_t i = 0; i < a->len; i++) {
		uint64_t av = a->limbs[i];
		uint64_t carry = 0;
		for(uint32_t j = 0; j < b->len; j++) {
			uint64_t cur = (uint64_t)r->limbs[i+j] + av * (uint64_t)b->limbs[j] + carry;
			r->limbs[i+j] = (uint32_t)(cur & 0xFFFFFFFFULL);
			carry = cur >> 32;
		}
		uint32_t k = i + b->len;
		while(carry) {
			uint64_t cur = (uint64_t)r->limbs[k] + carry;
			r->limbs[k] = (uint32_t)(cur & 0xFFFFFFFFULL);
			carry = cur >> 32;
			k++;
		}
	}
	r->len = n;
	r->sign = (a->sign == b->sign) ? 1 : -1;
	bn_normalize(r);
	return r;
}

/* b *= m (small multiplier, m <= 2^32-1) in place. */
static void bn_mul_small_inplace(bignum_t* b, uint32_t m) {
	if(b->len == 0)
		return;
	if(m == 0) { b->len = 0; b->sign = 0; return; }
	if(m == 1)
		return;
	bn_grow(b, b->len + 1);
	uint64_t carry = 0;
	for(uint32_t i = 0; i < b->len; i++) {
		uint64_t v = (uint64_t)b->limbs[i] * m + carry;
		b->limbs[i] = (uint32_t)(v & 0xFFFFFFFFULL);
		carry = v >> 32;
	}
	if(carry) { b->limbs[b->len] = (uint32_t)carry; b->len++; }
}

/* b += v (small addend) in place. */
static void bn_add_small_inplace(bignum_t* b, uint32_t v) {
	if(v == 0)
		return;
	bn_grow(b, b->len + 1);
	uint64_t carry = v;
	uint32_t i = 0;
	while(carry) {
		uint64_t s = (uint64_t)b->limbs[i] + carry;
		b->limbs[i] = (uint32_t)(s & 0xFFFFFFFFULL);
		carry = s >> 32;
		i++;
	}
	if(i > b->len)
		b->len = i;
	bn_normalize(b);
}

/* b /= d in place (small divisor 2..2^36); returns the remainder. */
static uint32_t bn_divmod_small_inplace(bignum_t* b, uint32_t d) {
	uint64_t rem = 0;
	for(uint32_t i = b->len; i > 0; i--) {
		uint64_t cur = (rem << 32) | (uint64_t)b->limbs[i-1];
		b->limbs[i-1] = (uint32_t)(cur / d);
		rem = cur % d;
	}
	bn_normalize(b);
	return (uint32_t)rem;
}

/* Binary long division of magnitudes: |a| / |b| -> (*q, *r), both non-negative.
 * Precondition b != 0. O(bits(a) * limbs) - fine for script-sized BigInts. */
static void bn_divmod_abs(const bignum_t* a, const bignum_t* b, bignum_t** q_out, bignum_t** r_out) {
	bignum_t* q = bn_new();
	bignum_t* rem = bn_new();
	if(bn_cmp_abs(a, b) < 0) {
		bn_free(rem);
		rem = bn_clone(a);
		if(rem->len) rem->sign = 1;
		*q_out = q; *r_out = rem;
		return;
	}
	bn_grow(q, a->len);
	bn_grow(rem, a->len + 2);
	uint32_t nbits = a->len * 32;
	for(uint32_t i = nbits; i > 0; i--) {
		uint32_t bit = i - 1;
		uint32_t limb = bit >> 5, off = bit & 31;
		/* rem = (rem << 1) | bit_of_a */
		if(rem->len) {
			uint32_t carry = 0;
			for(uint32_t k = 0; k < rem->len; k++) {
				uint32_t x = rem->limbs[k];
				rem->limbs[k] = (x << 1) | carry;
				carry = x >> 31;
			}
			if(carry) { rem->limbs[rem->len] = carry; rem->len++; }
		}
		if((a->limbs[limb] >> off) & 1) {
			rem->limbs[0] |= 1;
			if(rem->len == 0) rem->len = 1;
		}
		if(bn_cmp_abs(rem, b) >= 0) {
			bn_sub_abs_inplace(rem, b);
			q->limbs[limb] |= (1u << off);
			if(q->len <= limb) q->len = limb + 1;
		}
	}
	q->sign = q->len ? 1 : 0;
	bn_normalize(q);
	rem->sign = rem->len ? 1 : 0;
	bn_normalize(rem);
	*q_out = q; *r_out = rem;
}

bignum_t* bn_div(const bignum_t* a, const bignum_t* b) {
	if(bn_is_zero(b))
		return NULL;               // caller raises RangeError (division by zero)
	bignum_t *q, *r;
	bn_divmod_abs(a, b, &q, &r);
	bn_free(r);
	if(q->len)
		q->sign = (a->sign == b->sign) ? 1 : -1; // truncate toward zero
	return q;
}

bignum_t* bn_mod(const bignum_t* a, const bignum_t* b) {
	if(bn_is_zero(b))
		return NULL;
	bignum_t *q, *r;
	bn_divmod_abs(a, b, &q, &r);
	bn_free(q);
	if(r->len)
		r->sign = a->sign;         // remainder takes the sign of the dividend
	return r;
}

/* base ** exp by binary exponentiation. exp must be >= 0 (else NULL: JS throws
 * RangeError for a negative BigInt exponent). Squaring stops at exp's top bit
 * so the intermediate never exceeds base^(2^bitlen(exp)). */
bignum_t* bn_pow(const bignum_t* base, const bignum_t* exp) {
	if(exp->sign < 0)
		return NULL;
	bignum_t* result = bn_from_int64(1);
	if(exp->sign == 0)
		return result;             // x ** 0 == 1n
	int top = -1;
	for(uint32_t i = exp->len; i > 0 && top < 0; i--) {
		uint32_t x = exp->limbs[i-1];
		if(x != 0) {
			uint32_t bsr = 0;
			while(x >>= 1) bsr++;
			top = (int)((i-1) * 32 + bsr);
		}
	}
	bignum_t* b = bn_clone(base);
	for(int i = 0; i <= top; i++) {
		uint32_t limb = (uint32_t)i >> 5, off = (uint32_t)i & 31;
		if((exp->limbs[limb] >> off) & 1) {
			bignum_t* t = bn_mul(result, b);
			bn_free(result);
			result = t;
		}
		if(i < top) {
			bignum_t* sq = bn_mul(b, b);
			bn_free(b);
			b = sq;
		}
	}
	bn_free(b);
	return result;
}

/* Infinite two's-complement bitwise (AND/OR/XOR). Negative operands are treated
 * as ~(mag-1) with an infinite run of leading 1 bits; the result's fill limb
 * decides its sign, and a negative result is converted back to magnitude. */
static bignum_t* bn_bitwise(const bignum_t* a, const bignum_t* b, int which) {
	uint32_t n = (a->len > b->len) ? a->len : b->len;
	uint32_t a_fill = (a->sign < 0) ? 0xFFFFFFFFu : 0u;
	uint32_t b_fill = (b->sign < 0) ? 0xFFFFFFFFu : 0u;
	uint32_t r_fill;
	if(which == 0)      r_fill = a_fill & b_fill;
	else if(which == 1) r_fill = a_fill | b_fill;
	else                r_fill = a_fill ^ b_fill;

	uint32_t* A = (uint32_t*)mario_malloc((n + 1) * sizeof(uint32_t));
	uint32_t* B = (uint32_t*)mario_malloc((n + 1) * sizeof(uint32_t));
	/* Expand each operand to n+1 two's-complement limbs. A non-negative operand
	 * is zero-extended; a negative one is ~(mag-1) computed with a running
	 * borrow, which naturally yields 0xFFFFFFFF limbs past the magnitude (mag>=1
	 * always clears the borrow by the top limb). */
	uint32_t borrow = 1;
	for(uint32_t i = 0; i <= n; i++) {
		uint32_t x = (i < a->len) ? a->limbs[i] : 0u;
		if(a->sign < 0) { uint32_t y = x - borrow; borrow = (x < borrow) ? 1u : 0u; A[i] = ~y; }
		else A[i] = x;
	}
	A[n] = a_fill;
	borrow = 1;
	for(uint32_t i = 0; i <= n; i++) {
		uint32_t x = (i < b->len) ? b->limbs[i] : 0u;
		if(b->sign < 0) { uint32_t y = x - borrow; borrow = (x < borrow) ? 1u : 0u; B[i] = ~y; }
		else B[i] = x;
	}
	B[n] = b_fill;

	bignum_t* r = bn_new();
	bn_grow(r, n + 1);
	for(uint32_t i = 0; i <= n; i++) {
		uint32_t x = A[i], y = B[i];
		r->limbs[i] = (which == 0) ? (x & y) : (which == 1) ? (x | y) : (x ^ y);
	}
	mario_free(A);
	mario_free(B);
	r->len = n + 1;
	if(r_fill == 0xFFFFFFFFu) {
		/* negative result: magnitude = ~R + 1 */
		uint64_t carry = 1;
		for(uint32_t i = 0; i < r->len; i++) {
			uint64_t v = (uint64_t)(~r->limbs[i]) + carry;
			r->limbs[i] = (uint32_t)(v & 0xFFFFFFFFULL);
			carry = v >> 32;
		}
		if(carry) {
			bn_grow(r, r->len + 1);
			r->limbs[r->len] = (uint32_t)carry;
			r->len++;
		}
		r->sign = -1;
		bn_normalize(r);
	} else {
		r->sign = 1;
		bn_normalize(r);
	}
	return r;
}

bignum_t* bn_and(const bignum_t* a, const bignum_t* b) { return bn_bitwise(a, b, 0); }
bignum_t* bn_or (const bignum_t* a, const bignum_t* b) { return bn_bitwise(a, b, 1); }
bignum_t* bn_xor(const bignum_t* a, const bignum_t* b) { return bn_bitwise(a, b, 2); }

bignum_t* bn_shl(const bignum_t* a, uint32_t bits) {
	if(a->sign == 0 || bits == 0)
		return bn_clone(a);
	uint32_t wordshift = bits >> 5, bitshift = bits & 31;
	uint32_t n = a->len + wordshift + (bitshift ? 1 : 0) + 1;
	bignum_t* r = bn_new();
	bn_grow(r, n);
	uint32_t carry = 0;
	for(uint32_t i = 0; i < a->len; i++) {
		uint64_t x = (uint64_t)a->limbs[i];
		r->limbs[i + wordshift] = (uint32_t)((x << bitshift) | carry);
		carry = (bitshift == 0) ? 0 : (uint32_t)(x >> (32 - bitshift));
	}
	if(carry)
		r->limbs[a->len + wordshift] = carry;
	r->len = n;
	r->sign = a->sign;
	bn_normalize(r);
	return r;
}

/* Logical (magnitude) shift right; result is non-negative. */
static bignum_t* bn_shr_mag(const bignum_t* a, uint32_t bits) {
	bignum_t* r = bn_new();
	if(a->len == 0 || bits == 0)   // magnitude zero-check via len (sign may be unset on magnitudes)
		return (bits == 0) ? bn_clone(a) : r;
	uint32_t wordshift = bits >> 5, bitshift = bits & 31;
	if(wordshift >= a->len)
		return r;
	uint32_t n = a->len - wordshift;
	bn_grow(r, n);
	for(uint32_t i = 0; i < n; i++) {
		uint32_t src = i + wordshift;
		uint32_t lo = a->limbs[src];
		uint32_t hi = (bitshift && src + 1 < a->len) ? a->limbs[src+1] : 0;
		r->limbs[i] = (bitshift == 0) ? lo : ((lo >> bitshift) | (hi << (32 - bitshift)));
	}
	r->len = n;
	r->sign = 1;
	bn_normalize(r);
	return r;
}

/* Arithmetic shift right = floor(a / 2^bits). */
bignum_t* bn_shr(const bignum_t* a, uint32_t bits) {
	if(a->sign == 0 || bits == 0)
		return bn_clone(a);
	if(a->sign > 0)
		return bn_shr_mag(a, bits);
	/* negative: floor(a/2^k) = -( ((|a|-1) >> k) + 1 ) */
	bignum_t* mag = bn_clone(a); mag->sign = 1;
	bignum_t* one = bn_from_int64(1);
	bignum_t* mag1 = bn_sub_abs(mag, one);
	mag1->sign = mag1->len ? 1 : 0;   // bn_sub_abs leaves sign unset; make mag1 well-formed
	bn_free(mag);
	bignum_t* shifted = bn_shr_mag(mag1, bits);
	bn_free(mag1);
	bignum_t* plus1 = bn_add_abs(shifted, one);
	bn_free(shifted);
	bn_free(one);
	plus1->sign = plus1->len ? -1 : 0;
	bn_normalize(plus1);
	return plus1;
}

int64_t bn_to_int64(const bignum_t* b) {
	if(b->sign == 0)
		return 0;
	uint64_t lo = b->limbs[0];
	uint64_t hi = (b->len > 1) ? b->limbs[1] : 0;
	uint64_t mag = lo | (hi << 32);
	if(b->sign < 0)
		return (int64_t)(0ULL - mag);
	return (int64_t)mag;
}

double bn_to_double(const bignum_t* b) {
	if(b->sign == 0)
		return 0.0;
	double d = 0.0;
	for(uint32_t i = b->len; i > 0; i--)
		d = d * 4294967296.0 + (double)b->limbs[i-1];
	return (b->sign < 0) ? -d : d;
}

bignum_t* bn_from_double(double d) {
	/* Caller guarantees d is finite and integral (BigInt(x) throws otherwise). */
	if(d >= -9223372036854775808.0 && d < 9223372036854775808.0)
		return bn_from_int64((int64_t)d);
	int sign = (d < 0) ? -1 : 1;
	d = fabs(d);
	int exp = 0;
	double mant = frexp(d, &exp);                       // d = mant * 2^exp
	uint64_t significand = (uint64_t)(mant * 9007199254740992.0); // mant * 2^53
	int shift = exp - 53;
	bignum_t* b = bn_from_uint64(significand);
	bignum_t* r = (shift >= 0) ? bn_shl(b, (uint32_t)shift)
	                           : bn_shr_mag(b, (uint32_t)(-shift));
	bn_free(b);
	if(sign < 0 && r->len)
		r->sign = -1;
	return r;
}

int bn_cmp_double(const bignum_t* a, double d) {
	if(isnan(d))
		return 0;
	if(isinf(d))
		return (d > 0) ? -1 : 1;
	if(d == trunc(d) && d >= -9223372036854775808.0 && d < 9223372036854775808.0) {
		bignum_t* b = bn_from_int64((int64_t)d);
		int r = bn_cmp(a, b);
		bn_free(b);
		return r;
	}
	double da = bn_to_double(a);
	return (da < d) ? -1 : (da > d) ? 1 : 0;
}

/* a mod 2^bits, mathematical (result always in [0, 2^bits)). */
static bignum_t* bn_mod_pow2(const bignum_t* a, uint32_t bits) {
	bignum_t* r = bn_new();
	if(bits == 0)
		return r;
	uint32_t words = (bits + 31) >> 5;
	uint32_t lowbits = bits & 31;
	bn_grow(r, words);
	uint32_t n = (a->len < words) ? a->len : words;
	for(uint32_t i = 0; i < n; i++)
		r->limbs[i] = a->limbs[i];
	if(lowbits && n == words)
		r->limbs[words-1] &= (1u << lowbits) - 1;
	r->len = words;
	bn_normalize(r);
	if(a->sign < 0 && r->len != 0) {
		/* negative: result = 2^bits - (|a| mod 2^bits) = (~r + 1) within width */
		bn_grow(r, words);
		uint64_t carry = 1;
		for(uint32_t i = 0; i < words; i++) {
			uint64_t v = (uint64_t)(~r->limbs[i]) + carry;
			r->limbs[i] = (uint32_t)(v & 0xFFFFFFFFULL);
			carry = v >> 32;
		}
		if(lowbits)
			r->limbs[words-1] &= (1u << lowbits) - 1;
		r->len = words;
		bn_normalize(r);
	}
	r->sign = r->len ? 1 : 0;
	return r;
}

bignum_t* bn_asUintN(uint32_t bits, const bignum_t* a) {
	return bn_mod_pow2(a, bits);
}

bignum_t* bn_asIntN(uint32_t bits, const bignum_t* a) {
	if(bits == 0)
		return bn_new();
	bignum_t* r = bn_mod_pow2(a, bits);
	uint32_t topword = (bits - 1) >> 5, topoff = (bits - 1) & 31;
	bool high = (topword < r->len) && ((r->limbs[topword] >> topoff) & 1);
	if(high) {
		uint32_t words = (bits + 31) >> 5, lowbits = bits & 31;
		bn_grow(r, words);
		uint64_t carry = 1;
		for(uint32_t i = 0; i < words; i++) {
			uint64_t v = (uint64_t)(~r->limbs[i]) + carry;
			r->limbs[i] = (uint32_t)(v & 0xFFFFFFFFULL);
			carry = v >> 32;
		}
		if(lowbits)
			r->limbs[words-1] &= (1u << lowbits) - 1;
		r->len = words;
		bn_normalize(r);
		r->sign = r->len ? -1 : 0;
	}
	return r;
}

bignum_t* bn_from_string(const char* s, int radix) {
	bignum_t* r = bn_new();
	if(s == NULL)
		return r;
	while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;
	int sign = 1;
	if(*s == '+') s++;
	else if(*s == '-') { sign = -1; s++; }
	if(radix == 0) {
		radix = 10;
		if(s[0] == '0') {
			if(s[1] == 'x' || s[1] == 'X') { radix = 16; s += 2; }
			else if(s[1] == 'b' || s[1] == 'B') { radix = 2; s += 2; }
			else if(s[1] == 'o' || s[1] == 'O') { radix = 8; s += 2; }
		}
	} else if(radix == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
	}
	if(radix < 2 || radix > 36)
		radix = 10;
	bool any = false;
	for(; *s; s++) {
		char c = *s;
		if(c == '_')
			continue;
		int dv;
		if(c >= '0' && c <= '9') dv = c - '0';
		else if(c >= 'a' && c <= 'z') dv = c - 'a' + 10;
		else if(c >= 'A' && c <= 'Z') dv = c - 'A' + 10;
		else break;
		if(dv >= radix)
			break;
		any = true;
		bn_mul_small_inplace(r, (uint32_t)radix);
		bn_add_small_inplace(r, (uint32_t)dv);
	}
	if(any)
		r->sign = r->len ? sign : 0;
	return r;
}

void bn_to_mstr(const bignum_t* b, int radix, mstr_t* out) {
	if(radix < 2 || radix > 36)
		radix = 10;
	if(b->sign == 0) {
		mstr_add(out, '0');
		return;
	}
	if(b->sign < 0)
		mstr_add(out, '-');
	bignum_t* mag = bn_clone(b);
	mag->sign = 1;
	uint32_t cap = mag->len * 32 + 4;
	char* digits = (char*)mario_malloc(cap);
	uint32_t di = 0;
	while(mag->len > 0 && di < cap)
		digits[di++] = "0123456789abcdefghijklmnopqrstuvwxyz"[bn_divmod_small_inplace(mag, (uint32_t)radix)];
	for(uint32_t i = di; i > 0; i--)
		mstr_add(out, digits[i-1]);
	mario_free(digits);
	bn_free(mag);
}

/**======bytecode functions======*/

#define BC_BUF_SIZE  3232


uint32_t bc_getstrindex(bytecode_t* bc, const char* str) {
	uint32_t sz = bc->mstr_table.size;
	uint32_t i;
	if(str == NULL || str[0] == 0)
		return OFF_MASK;

	for(i=0; i<sz; ++i) {
		char* s = (char*)bc->mstr_table.items[i];
		if(s != NULL && strcmp(s, str) == 0)
			return i;
	}

	uint32_t len = (uint32_t)strlen(str);
	char* p = (char*)mario_malloc(len + 1);
	memcpy(p, str, len+1);
	array_add(&bc->mstr_table, p);
	return sz;
}	

void bc_init(bytecode_t* bc) {
	bc->cindex = 0;
	bc->code_buf = NULL;
	bc->buf_size = 0;
	array_init(&bc->mstr_table);
}

void bc_release(bytecode_t* bc) {
	array_clean(&bc->mstr_table, NULL);
	if(bc->code_buf != NULL)
		mario_free(bc->code_buf);
}

void bc_add(bytecode_t* bc, PC ins) {
	if(bc->cindex >= bc->buf_size) {
		bc->buf_size = bc->cindex + BC_BUF_SIZE;
		PC *new_buf = (PC*)mario_malloc(bc->buf_size*sizeof(PC));

		if(bc->cindex > 0 && bc->code_buf != NULL) {
			memcpy(new_buf, bc->code_buf, bc->cindex*sizeof(PC));
			mario_free(bc->code_buf);
		}
		bc->code_buf = new_buf;
	}

	bc->code_buf[bc->cindex] = ins;
	bc->cindex++;
}
	
PC bc_reserve(bytecode_t* bc) {
	bc_add(bc, INS(INSTR_NIL, OFF_MASK));
  return bc->cindex-1;
}

PC bc_bytecode(bytecode_t* bc, opr_code_t instr, const char* str) {
	opr_code_t r = instr;
	uint32_t i = OFF_MASK;

	if(str != NULL && str[0] != 0)
		i = bc_getstrindex(bc, str);

	return INS(r, i);
}
	
PC bc_gen_int(bytecode_t* bc, opr_code_t instr, int32_t i) {
	PC ins = bc_bytecode(bc, instr, "");
	bc_add(bc, ins);
	bc_add(bc, i);
	return bc->cindex;
}

PC bc_gen_short(bytecode_t* bc, opr_code_t instr, int32_t s) {
	PC ins = bc_bytecode(bc, instr, "");
	ins = (ins&0xFFF0000) | (s&OFF_MASK);
	bc_add(bc, ins);
	return bc->cindex;
}
	
PC bc_gen_str(bytecode_t* bc, opr_code_t instr, const char* str) {
	uint32_t i = 0;
	const char* s = str;
	int64_t ll = 0;
	double dd = 0.0;

	if(instr == INSTR_INT) {
		s = NULL;
		if(strstr(str, "0x") != NULL || strstr(str, "0X") != NULL) {
			/* Hex literals are non-negative in JS. Fit int32 -> INSTR_INT, else
			 * int64 -> INSTR_INT64, else the exact double -> INSTR_FLOAT64. */
			errno = 0;
			unsigned long long h = strtoull(str, NULL, 16);
			if(errno == ERANGE || h > 0x7FFFFFFFFFFFFFFFULL) {
				dd = strtod(str, NULL);
				instr = INSTR_FLOAT64;
			}
			else if(h > 0x7FFFFFFFULL) {
				ll = (int64_t)h;
				instr = INSTR_INT64;
			}
			else {
				i = (uint32_t)h; // stays INSTR_INT
			}
		}
		else {
			/* A decimal integer is stored at the narrowest width that holds it
			 * exactly: int32 -> INSTR_INT/INSTR_INT_S, int64 -> INSTR_INT64, and
			 * anything wider (strtoll saturates with ERANGE) -> the double the
			 * literal denotes via INSTR_FLOAT64. No silent int32 truncation. */
			errno = 0;
			ll = strtoll(str, NULL, 10);
			if(errno == ERANGE) {
				dd = strtod(str, NULL);
				instr = INSTR_FLOAT64;
			}
			else if(ll < -2147483648LL || ll > 2147483647LL) {
				instr = INSTR_INT64;
			}
			else {
				i = (uint32_t)(int)ll; // stays INSTR_INT
			}
		}
	}
	else if(instr == INSTR_FLOAT) {
		/* Float literals are canonical doubles now (V_FLOAT64); the lossy
		 * float32 1-word path is dropped. Math.fround is the only runtime
		 * producer of float32, never a literal. */
		dd = strtod(str, NULL);
		instr = INSTR_FLOAT64;
		s = NULL;
	}

	PC ins = bc_bytecode(bc, instr, s);
	bc_add(bc, ins);

	if(instr == INSTR_INT) {
		if(i < OFF_MASK) //short int
			bc->code_buf[bc->cindex-1] = INS(INSTR_INT_S, i);
		else
			bc_add(bc, i);
	}
	else if(instr == INSTR_INT64) {
		/* 8-byte payload rides along as 2 consecutive PC words; emit and decode
		 * are symmetric so host byte order is irrelevant. */
		uint32_t words[2];
		memcpy(words, &ll, sizeof(words));
		bc_add(bc, words[0]);
		bc_add(bc, words[1]);
	}
	else if(instr == INSTR_FLOAT64) {
		uint32_t words[2];
		memcpy(words, &dd, sizeof(words));
		bc_add(bc, words[0]);
		bc_add(bc, words[1]);
	}
	return bc->cindex;
}

PC bc_gen(bytecode_t* bc, opr_code_t instr) {
	return bc_gen_str(bc, instr, "");
}

void bc_remove_instr(bytecode_t* bc, PC from, uint32_t num) {
	PC off = from+num;
	while(off < bc->cindex) {
		bc->code_buf[off-num] = bc->code_buf[off];
		off++;
	}
	bc->cindex -= num;
}

void bc_set_instr(bytecode_t* bc, PC anchor, opr_code_t op, PC target) {
	if(target == ILLEGAL_PC)
		target = bc->cindex;

	int offset = target > anchor ? (target-anchor) : (anchor-target);
	PC ins = INS(op, offset);
	bc->code_buf[anchor] = ins;
}

PC bc_add_instr(bytecode_t* bc, PC anchor, opr_code_t op, PC target) {
	if(target == ILLEGAL_PC)
		target = bc->cindex;

	int offset = target > anchor ? (target-anchor) : (anchor-target);
	PC ins = INS(op, offset);
	bc_add(bc, ins);
	return bc->cindex;
} 

/** var cache for const value --------------*/

static void var_cache_init(vm_t* vm) {
    uint32_t i;
	if(vm->var_cache.size == 0)
		return;

	vm->var_cache.cache = (var_t**)mario_malloc(vm->var_cache.size*sizeof(var_t*));
    for(i=0; i<vm->var_cache.size; ++i) {
        vm->var_cache.cache[i] = NULL;
    }
    vm->var_cache.used = 0;
}

static void var_cache_free(vm_t* vm) {
	if(vm->var_cache.size == 0)
		return;

    uint32_t i;
    for(i=0; i<vm->var_cache.used; ++i) {
        var_t* v = vm->var_cache.cache[i];
        var_unref(v);
        vm->var_cache.cache[i] = NULL;
    }
    vm->var_cache.used = 0;
	mario_free(vm->var_cache.cache);
}

static int32_t var_cache(vm_t* vm, var_t* v) {
    if(vm->var_cache.used >= vm->var_cache.size)
        return -1;
    vm->var_cache.cache[vm->var_cache.used] = var_ref(v);
    vm->var_cache.used++;
    return vm->var_cache.used-1;
}

static bool try_var_cache(vm_t* vm, PC* ins, var_t* v) {
	if(vm->var_cache.size == 0)
		return false;

    if((*ins) & INSTR_OPT_CACHE) {
        int index = var_cache(vm, v); 
        if(index >= 0) 
            *ins = INS(INSTR_CACHE, index);
        return true;
    }

    *ins = (*ins) | INSTR_OPT_CACHE;
    return false;
}

static void load_ncache_init(vm_t* vm) {
	if(vm->load_ncache.size == 0)
		return;

	vm->load_ncache.cache = (load_ncache_t*)mario_malloc(vm->load_ncache.size*sizeof(load_ncache_t));
    uint32_t i;
    for(i=0; i<vm->load_ncache.size; ++i) {
        memset(&vm->load_ncache.cache[i], 0, sizeof(load_ncache_t));
    }
}

static void load_ncache_free(vm_t* vm) {
	if(vm->load_ncache.size == 0)
		return;

	mario_free(vm->load_ncache.cache);
	/* Zero the cache so the later var-graph teardown in vm_close() (node_free ->
	 * load_ncache_invalidate) early-returns on the size==0 guard instead of
	 * reading the freed array. */
	vm->load_ncache.cache = NULL;
	vm->load_ncache.size = 0;
}

static void load_ncache_invalidate(vm_t* vm, node_t* node) {
	/* A node whose var carries no VM (node_free -> load_ncache_invalidate passes
	 * node->var->vm, which is NULL for a var that was never bound to a running
	 * VM) has no cache to invalidate; guard before touching vm->load_ncache. */
	if(vm == NULL || vm->load_ncache.size == 0)
		return;

	if(node == NULL || node->ncache_instr == 0)
        return;

    uint32_t i = (node->ncache_instr & OFF_MASK);
	if(i >= LOAD_NCACHE_MAX_DEF)
		return;

    load_ncache_t* l = &vm->load_ncache.cache[i];
    if(l->node != node || l->old_instr_pcs == NULL) 
		return;

	PC* code = vm->bc.code_buf;
	uint32_t j;
	for(j=0; j<l->old_instr_pcs->size; ++j) {
		PC pc = *(PC*)array_get(l->old_instr_pcs, j);
		code[pc] = l->old_instr;
	}
	array_free(l->old_instr_pcs, NULL);
    memset(l, 0, sizeof(load_ncache_t));
}

/* Self-modified NCACHE loads are only undone when the cached node dies
 * (node_free -> load_ncache_invalidate). A scope var captured by a closure
 * (block-scope capture / "@@lex" links) outlives its scope, so its nodes stay
 * alive and the next iteration / next run of the same code would read and
 * write the PREVIOUS binding through the stale cache. Undo every cache entry
 * owned by such a surviving scope var when its scope pops. */
static void load_ncache_invalidate_var(vm_t* vm, var_t* var) {
	uint32_t i;
	if(vm->load_ncache.size == 0 || var == NULL || var->children.buckets == NULL)
		return;
	for(i=0; i<vm->load_ncache.size; ++i) {
		load_ncache_t* l = &vm->load_ncache.cache[i];
		if(l->node == NULL || l->node->name == NULL)
			continue;
		if(hash_map_get(&var->children, l->node->name) == l->node)
			load_ncache_invalidate(vm, l->node);
	}
}

static void load_ncache(vm_t* vm, node_t* node, PC instr_pc) {
	if(vm->load_ncache.size == 0 || node == NULL)
		return;

	PC* code = vm->bc.code_buf;
	if(node->ncache_instr != 0) {
    	uint32_t i = (node->ncache_instr & OFF_MASK);
        load_ncache_t* l = &vm->load_ncache.cache[i];	
		if(l->node != node || l->old_instr_pcs == NULL)
			return;

		code[instr_pc] = node->ncache_instr;
		PC* pc = (PC*)mario_malloc(sizeof(PC));
		*pc = instr_pc;
		array_add(l->old_instr_pcs, (void*)pc);
		return;
	}

	uint32_t i;
    for(i=0; i<vm->load_ncache.size; ++i) {
        load_ncache_t* l = &vm->load_ncache.cache[i];	
        if(l->node == NULL) {
			node->ncache_instr = INS(INSTR_NCACHE, i);
			l->old_instr_pcs = array_new();
			l->node = node;

			PC* pc = (PC*)mario_malloc(sizeof(PC));
			*pc = instr_pc;
			array_add(l->old_instr_pcs, (void*)pc);
			l->old_instr = code[instr_pc];
			code[instr_pc] = node->ncache_instr;
			break;
        }
    }
}

/**======var functions======*/
load_m_func_t _load_m_func = NULL;

static var_t* var_clone(var_t* v) {
	switch(v->type) { //basic types
		case V_INT:
			return var_new_int(v->vm, var_get_int(v));
		case V_FLOAT:
			return var_new_float(v->vm, var_get_float(v));
		case V_STRING:
			return var_new_str(v->vm, var_get_str(v));
		/*case V_BOOL:
			return var_new_bool(v->vm, var_get_bool(v));
		case V_NULL:
			return var_new_null(v->vm);
		case V_UNDEF:
			return var_new(v->vm);*/
		default:
			break;
	}
	//object types
	return v; 
}

node_t* node_new(vm_t* vm, const char* name, var_t* var) {
	node_t* node = (node_t*)mario_malloc(sizeof(node_t));
	memset(node, 0, sizeof(node_t));

	node->magic = 1;
	uint32_t len = (uint32_t)strlen(name);
	node->name = (char*)mario_malloc(len+1);
	memcpy(node->name, name, len+1);
	if(var != NULL)
		//node->var = var_ref(var_clone(var));
		node->var = var_ref(var);
	else
		node->var = var_ref(var_new(vm));

	if(strcmp(name, CONSTRUCTOR) == 0)
		node->be_unenumerable = true;
	return node;
}

static inline bool var_empty(var_t* var) {
	if(var == NULL || var->status <= V_ST_GC_FREE)
		return true;
	return false;
}

void node_free(void* p) {
	node_t* node = (node_t*)p;
	if(node == NULL)
		return;
	
	if(node->var != NULL && node->var->vm != NULL) {
		load_ncache_invalidate(node->var->vm, node);
	}


	if(!var_empty(node->var) && node->var->vm != NULL) {
		var_unref(node->var);
	}
	mario_free(node->name);
	mario_free(node);
}

static inline bool node_empty(node_t* node) {
	if(node == NULL || var_empty(node->var))
		return true;
	return false;
}

inline var_t* node_replace(node_t* node, var_t* v) {
	var_t* old = node->var;
	//node->var = var_ref(var_clone(v));
	node->var = var_ref(v);
	var_unref(old);
	return v;
}

inline void var_remove_all(var_t* var) {
	/*free children*/
	hash_map_clean(&var->children, mario_free, (free_func_t)node_free);
}

static inline node_t* var_find_raw(var_t* var, const char*name) {
	if(var_empty(var))
		return NULL;

	// Use hash map to find the node
	return (node_t*)hash_map_get(&var->children, name);
}

node_t* var_add(var_t* var, const char* name, var_t* add) {
	node_t* node = NULL;

	if(name[0] != 0) 
		node = var_find_raw(var, name);

	if(node == NULL) {
		node = node_new(var->vm, name, add);
		hash_map_add(&var->children, name, node);
	}
	else if(add != NULL)
		node_replace(node, add);

	return node;
}

node_t* var_add_head(var_t* var, const char* name, var_t* add) {
	return var_add(var, name, add);
}

inline node_t* var_find_own_member(var_t* var, const char*name) {
	node_t* node = var_find_raw(var, name);
	if(node_empty(node))
		return NULL;
	return node;
}

inline var_t* var_find_own_member_var(var_t* var, const char*name) {
	node_t* node = var_find_own_member(var, name);
	if(node != NULL) {
		return node->var;
	}
	return NULL;
}

/* ES6 Symbol detection: a symbol instance carries a hidden own marker member
 * SYM_MARKER whose string value is its unique property key (see mario.h). */
bool var_is_symbol(var_t* var) {
	return var != NULL && var->type == V_OBJECT && !var->is_array && !var->is_func &&
		var_find_own_member(var, SYM_MARKER) != NULL;
}

/* The property-key string a symbol maps to (NULL for non-symbols). Used by
 * member access so `obj[symbol]` and `{[symbol]: v}` resolve consistently. */
const char* var_symbol_key(var_t* var) {
	var_t* k = var_find_own_member_var(var, SYM_MARKER);
	return (k != NULL && k->type == V_STRING) ? var_get_str(k) : NULL;
}

/* Exotic-object markers: an exotic object (ArrayBuffer / SharedArrayBuffer /
 * TypedArray / DataView / Proxy) is an ordinary V_OBJECT carrying a hidden own
 * member EXOTIC_MARKER whose string value is its kind. var_is_exotic() is the
 * single discriminator the property-access intercept (Phases 3-5) gates on; for
 * a plain object it is one hash miss, so the intercept stays a genuine no-op. */
const char* var_exotic_kind(var_t* var) {
	if(var == NULL || var->type != V_OBJECT || var->is_array || var->is_func)
		return NULL;
	var_t* k = var_find_own_member_var(var, EXOTIC_MARKER);
	return (k != NULL && k->type == V_STRING) ? var_get_str(k) : NULL;
}

bool var_is_exotic(var_t* var) {
	return var_exotic_kind(var) != NULL;
}

bool var_is_arraybuffer(var_t* var) {
	const char* k = var_exotic_kind(var);
	return k != NULL && (strcmp(k, EXOTIC_ARRAYBUFFER) == 0 || strcmp(k, EXOTIC_SHARED) == 0);
}

bool var_is_typedarray(var_t* var) {
	const char* k = var_exotic_kind(var);
	return k != NULL && strcmp(k, EXOTIC_TYPEDARRAY) == 0;
}

bool var_is_dataview(var_t* var) {
	const char* k = var_exotic_kind(var);
	return k != NULL && strcmp(k, EXOTIC_DATAVIEW) == 0;
}

bool var_is_proxy(var_t* var) {
	const char* k = var_exotic_kind(var);
	return k != NULL && strcmp(k, EXOTIC_PROXY) == 0;
}

/* ====== TypedArray element access ======
 * A TypedArray is a V_OBJECT with hidden marker @@exotic="ta", a hidden @@etype
 * (TA_* code), and JS-readable unenumerable `buffer` (the shared ArrayBuffer),
 * `byteOffset`, `byteLength`, `length`, `BYTES_PER_ELEMENT`. The buffer's raw
 * bytes are the single source of truth so DataView and multiple TypedArray views
 * over one ArrayBuffer stay live. Element access is host-endian and memcpy-safe
 * (no unaligned deref); get_at returns a fresh decoded var (OOB -> NULL, which
 * the caller renders as undefined), set_at encodes with clamping/wrapping/BigInt
 * per the etype and returns false on OOB or a detached/mis-typed receiver. */
static const uint32_t ta_elem_size[TA_ETYPE_COUNT] = { 1,1,1,2,2,4,4,4,8,8,8 };

/* ToNumber for the write path: strings via strtod (JS `ta[0]="5"` -> 5), other
 * types via the shared numeric coercion. BigInt is handled by the caller. */
static double ta_to_number(var_t* v) {
	if(v == NULL)
		return 0.0;
	if(v->type == V_STRING) {
		const char* s = var_get_str(v);
		while(*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
		if(*s == 0) return 0.0;
		char* end = NULL;
		double d = strtod(s, &end);
		while(end != NULL && (*end==' '||*end=='\t'||*end=='\n'||*end=='\r')) end++;
		if(end != NULL && *end != 0) return NAN;
		return d;
	}
	return var_get_float64(v);
}

/* ToIntegerOrInfinity then saturate into int64 (NaN/Inf -> 0, matching V8 for
 * integer-view stores); the caller masks to the target width for the wrap. */
static int64_t ta_to_integer(double d) {
	if(isnan(d) || isinf(d)) return 0;
	if(d >=  9223372036854775807.0) return INT64_MAX;
	if(d <= -9223372036854775807.0) return INT64_MIN;
	return (int64_t)d; /* truncates toward zero */
}

static uint8_t* ta_elem_ptr(var_t* ta, int64_t idx, int et, uint8_t** out_base) {
	var_t* buf = var_find_own_member_var(ta, "buffer");
	if(buf == NULL || !var_is_arraybuffer(buf) || buf->value == NULL)
		return NULL;
	int64_t len = var_get_int64(var_find_own_member_var(ta, "length"));
	if(idx < 0 || idx >= len)
		return NULL; /* OOB */
	uint32_t esz = ta_elem_size[et];
	uint32_t off = (uint32_t)var_get_int64(var_find_own_member_var(ta, "byteOffset"));
	uint8_t* p = (uint8_t*)buf->value + off + (uint32_t)idx * esz;
	if(out_base != NULL) *out_base = p;
	return p;
}

var_t* var_typedarray_get_at(vm_t* vm, var_t* ta, int64_t idx) {
	if(ta == NULL || !var_is_typedarray(ta))
		return NULL;
	int et = var_get_int(var_find_own_member_var(ta, TA_ETYPE));
	if(et < 0 || et >= TA_ETYPE_COUNT)
		return NULL;
	uint8_t* p = ta_elem_ptr(ta, idx, et, NULL);
	if(p == NULL)
		return NULL; /* OOB read -> undefined */
	switch(et) {
		case TA_INT8:         { int8_t   v; memcpy(&v,p,1); return var_new_int(vm, (int)v); }
		case TA_UINT8:
		case TA_UINT8CLAMPED: { uint8_t  v; memcpy(&v,p,1); return var_new_int(vm, (int)v); }
		case TA_INT16:        { int16_t  v; memcpy(&v,p,2); return var_new_int(vm, (int)v); }
		case TA_UINT16:       { uint16_t v; memcpy(&v,p,2); return var_new_int(vm, (int)v); }
		case TA_INT32:        { int32_t  v; memcpy(&v,p,4); return var_new_int(vm, (int)v); }
		case TA_UINT32:       { uint32_t v; memcpy(&v,p,4); return var_new_int64(vm, (int64_t)v); }
		case TA_FLOAT32:      { float    v; memcpy(&v,p,4); return var_new_float64(vm, (double)v); }
		case TA_FLOAT64:      { double   v; memcpy(&v,p,8); return var_new_float64(vm, v); }
		case TA_BIGINT64:     { uint64_t v; memcpy(&v,p,8); return var_new_bigint(vm, bn_from_int64((int64_t)v)); }
		case TA_BIGUINT64:    { uint64_t v; memcpy(&v,p,8); return var_new_bigint(vm, bn_from_uint64(v)); }
		default:              return NULL;
	}
}

bool var_typedarray_set_at(vm_t* vm, var_t* ta, int64_t idx, var_t* val) {
	(void)vm;
	if(ta == NULL || !var_is_typedarray(ta))
		return false;
	int et = var_get_int(var_find_own_member_var(ta, TA_ETYPE));
	if(et < 0 || et >= TA_ETYPE_COUNT)
		return false;
	uint8_t* p = ta_elem_ptr(ta, idx, et, NULL);
	if(p == NULL)
		return false; /* OOB write -> ignored (non-strict) */
	switch(et) {
		case TA_INT8:         { int8_t   v=(int8_t)  ta_to_integer(ta_to_number(val)); memcpy(p,&v,1); break; }
		case TA_UINT8:        { uint8_t  v=(uint8_t) ta_to_integer(ta_to_number(val)); memcpy(p,&v,1); break; }
		case TA_UINT8CLAMPED: {
			double d = ta_to_number(val);
			uint8_t v;
			if(isnan(d) || d <= 0.0) v = 0;
			else if(isinf(d) || d >= 255.0) v = 255;
			else {
				double fl = floor(d), diff = d - fl;
				if(diff > 0.5) v = (uint8_t)(fl + 1.0);
				else if(diff < 0.5) v = (uint8_t)fl;
				else v = (uint8_t)((fmod(fl,2.0)==0.0) ? fl : fl + 1.0); /* tie -> even */
			}
			memcpy(p,&v,1); break;
		}
		case TA_INT16:        { int16_t  v=(int16_t) ta_to_integer(ta_to_number(val)); memcpy(p,&v,2); break; }
		case TA_UINT16:       { uint16_t v=(uint16_t)ta_to_integer(ta_to_number(val)); memcpy(p,&v,2); break; }
		case TA_INT32:        { int32_t  v=(int32_t) ta_to_integer(ta_to_number(val)); memcpy(p,&v,4); break; }
		case TA_UINT32:       { uint32_t v=(uint32_t)ta_to_integer(ta_to_number(val)); memcpy(p,&v,4); break; }
		case TA_FLOAT32:      { float    v=(float)   ta_to_number(val);                memcpy(p,&v,4); break; }
		case TA_FLOAT64:      { double   v=          ta_to_number(val);                memcpy(p,&v,8); break; }
		case TA_BIGINT64:
		case TA_BIGUINT64:    {
			bignum_t* b = var_get_bigint(val);
			uint64_t v = (b != NULL) ? (uint64_t)bn_to_int64(b)
			                         : (uint64_t)ta_to_integer(ta_to_number(val));
			memcpy(p,&v,8); break;
		}
		default: return false;
	}
	return true;
}


/* `delete obj.name`: unlink and free the own member node. JS non-strict
 * semantics - deleting an own configurable property OR a non-existent one both
 * yield true (this VM does not model non-configurability). Proxy deleteProperty
 * interception is layered on in Phase 5; for plain/TypedArray/ArrayBuffer
 * objects this plain removal is exactly right. */
bool var_delete_own_member(var_t* obj, const char* name) {
	if(obj == NULL || name == NULL)
		return true;
	node_t* node = (node_t*)hash_map_remove(&obj->children, name);
	if(node != NULL)
		node_free(node);
	return true;
}

/* `name in obj`: own OR prototype-chain presence. Arrays keep their elements in
 * the nested _ARRAY_ store, so a numeric index is looked up there; "length" is
 * a virtual own property of arrays/strings (do_get computes it on the fly). */
bool var_has_member(var_t* obj, const char* name) {
	if(obj == NULL || name == NULL)
		return false;
	if(obj->is_array) {
		if(strcmp(name, "length") == 0)
			return true;
		var_t* arr = var_find_own_member_var(obj, "_ARRAY_");
		if(arr != NULL && hash_map_get(&arr->children, name) != NULL)
			return true;
	}
	else if(obj->type == V_STRING && strcmp(name, "length") == 0) {
		return true;
	}
	return var_find_member(obj, name) != NULL;
}

node_t* var_find_member(var_t* obj, const char* name) {
	node_t* node = var_find_own_member(obj, name);
	if(node == NULL)
		node = vm_find_in_class(obj, name);
	return node;
}

inline var_t* var_find_member_var(var_t* var, const char*name) {
	node_t* node = var_find_member(var, name);
	if(node != NULL) {
		return node->var;
	}
	return NULL;
}

inline node_t* var_find_member_create(var_t* var, const char*name) {
	node_t* n = var_find_member(var, name);
	if(n != NULL)
		return n;
	n = var_add(var, name, NULL);
	return n;
}

node_t* var_get(var_t* var, int32_t index) {
	// Note: This function is deprecated for hash map implementation
	// It's kept for backward compatibility but may not work correctly
	return NULL;
}

node_t* var_array_get(var_t* var, int32_t index) {
	var_t* arr_var = var;
	if(var->is_array)
	arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var == NULL)
		return NULL;

	// Convert index to string key
	char key[32];
	snprintf(key, sizeof(key), "%d", index);
	
	// Check if the key exists, if not add empty nodes up to the index
	int32_t i;
	for(i=0; i<=index; i++) {
		char current_key[32];
		snprintf(current_key, sizeof(current_key), "%d", i);
		if(hash_map_get(&arr_var->children, current_key) == NULL) {
			var_add(arr_var, current_key, NULL);
		}
	}

	node_t* node = (node_t*)hash_map_get(&arr_var->children, key);
	if(node_empty(node))
		return NULL;
	return node;
}

var_t* var_array_get_var(var_t* var, int32_t index) {
	node_t* n = var_array_get(var, index);
	if(n != NULL)
		return n->var;
	return NULL;
}

node_t* var_array_set(var_t* var, int32_t index, var_t* set_var) {
	node_t* node = var_array_get(var, index);
	if(node != NULL)
		node_replace(node, set_var);
	return node;
}

node_t* var_array_add(var_t* var, var_t* add_var) {
	node_t* ret = NULL;
	var_t* arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var != NULL) {
		// Get current size to use as next index
		uint32_t index = hash_map_size(&arr_var->children);
		// Convert index to string key
		char key[32];
		snprintf(key, sizeof(key), "%u", index);
		ret = var_add(arr_var, key, add_var);
	}
	return ret;
}

node_t* var_array_add_head(var_t* var, var_t* add_var) {
	node_t* ret = NULL;
	var_t* arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var != NULL)
		ret = var_add_head(arr_var, "", add_var);
	return ret;
}

uint32_t var_array_size(var_t* var) {
	var_t* arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var == NULL)
		return 0;
	return hash_map_size(&arr_var->children);
}

void var_array_reverse(var_t* arr) {
	uint32_t sz = var_array_size(arr);
	uint32_t i;
	for(i=0; i<sz/2; ++i) {
		node_t* n1 = var_array_get(arr, i);
		node_t* n2 = var_array_get(arr, sz-i-1);
		if(n1 != NULL && n2 != NULL) {
			var_t* tmp = n1->var;
			n1->var = n2->var;
			n2->var = tmp;
		}
	}
}

node_t* var_array_remove(var_t* var, int32_t index) {
	var_t* arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var == NULL)
		return NULL;
	
	// Convert index to string key
	char key[32];
	snprintf(key, sizeof(key), "%d", index);
	
	return (node_t*)hash_map_remove(&arr_var->children, key);
}

void var_array_del(var_t* var, int32_t index) {
	var_t* arr_var = var_find_own_member_var(var, "_ARRAY_");
	if(arr_var == NULL)
		return;
	
	// Convert index to string key
	char key[32];
	snprintf(key, sizeof(key), "%d", index);
	
	// Remove the node from hash map
	node_t* node = (node_t*)hash_map_remove(&arr_var->children, key);
	if(node != NULL) {
		node_free(node);
	}
}

/* Phase 6 weak-reference machinery (defined further down, after gc()). Forward
 * declared here because var_clean() and gc_vars() both consult it. */
static void vm_weak_target_dying(vm_t* vm, var_t* target);
static void gc_mark_weak(vm_t* vm, bool mark);

inline void var_clean(var_t* var) {
	if(var_empty(var))
		return;
	var->status = V_ST_FREE; //mark as freed for avoid dead loop

	vm_t* vm = var->vm;
	if(vm != NULL)
		vm->gc.gc_defer++; //defer opportunistic gc until this object graph is fully torn down.

	if(var->on_destroy != NULL) {
		var->on_destroy(var);
	}

	/* Phase 6: this var is being torn down. If it is the target of any WeakRef,
	 * clear those references (so deref() now returns undefined) and move any
	 * FinalizationRegistry cell observing it onto the pending queue, to be drained
	 * by the hidden gc() global at a safe point. Gated on a non-empty registry so a
	 * program without weak references pays a single pointer test per teardown. The
	 * target's identity is matched by pointer, which stays valid until var_free()
	 * recycles this var into the free pool AFTER var_clean() returns. */
	if(vm != NULL && vm->weak_cells != NULL)
		vm_weak_target_dying(vm, var);

	/*free children*/
	var_remove_all(var);	

	/*free value*/
	if(var->value != NULL) {
		if(var->free_func != NULL) 
			var->free_func(var->value);
		else
			mario_free(var->value);
		var->value = NULL;
	}

	var_t* next = var->next; //backup next
	var_t* prev = var->prev; //backup prev
	memset(var, 0, sizeof(var_t));
	var->next = next;
	var->prev = prev;

	if(vm != NULL)
		vm->gc.gc_defer--;
}

static inline void add_to_free(var_t* var) {
	vm_t* vm = var->vm;
	/* A var with no VM is already recycled memory reached through a dangling
	 * node->var; linking it into a free list would corrupt that list. */
	if(vm == NULL)
		return;
	var->status = V_ST_FREE;
	if(vm->free_var_buffer != NULL)
		vm->free_var_buffer->prev = var;
	var->next = vm->free_var_buffer;
	vm->free_var_buffer = var;
	vm->free_var_buffer_num++;
}

static void gc(vm_t* vm, bool force);
static inline void add_to_gc(var_t* var) {
	vm_t* vm = var->vm;
	/* Same sentinel as add_to_free: vm==NULL means this "var" is recycled heap
	 * (a dangling node->var), not a live variable. Dereferencing vm->gc here
	 * faulted at NULL+offsetof(gc) during Array.sort teardown. */
	if(vm == NULL)
		return;
	var->prev = vm->gc.gc_vars_tail;
	if(vm->gc.gc_vars_tail != NULL)
		vm->gc.gc_vars_tail->next = var;
	else {
		vm->gc.gc_vars = var;
	}
	var->next = NULL;
	vm->gc.gc_vars_tail = var;
	var->status = V_ST_GC;
	vm->gc.gc_vars_num++;

	/* Do not trigger a gc while an object graph is being torn down (var_clean):
	 * the re-entrant gc's gc_free_free_vars() would mario_free() vars that pending
	 * node_free() calls in the same teardown still dereference (node->var->vm),
	 * causing a use-after-free. Defer it to the next add_to_gc outside a teardown. */
	if(vm->gc.gc_vars_num > vm->gc.gc_trig_var_num && vm->gc.gc_defer == 0)
		vm->gc.gc_pending = true; /* consumed at the next vm_run safe point */
}

static inline var_t* get_from_free(vm_t* vm) {
	if(vm->gc.is_doing_gc)
		return NULL;

	var_t* var = vm->free_var_buffer;
	if(var != NULL) {
		vm->free_var_buffer = var->next;
		if(vm->free_var_buffer != NULL)
			vm->free_var_buffer->prev = NULL;
		if(vm->free_var_buffer_num > 0)
			vm->free_var_buffer_num--;
	}
	return var;
}

static inline void remove_from_gc(var_t* var) {
	vm_t* vm = var->vm;
	/* Recycled heap reached through a dangling node->var can read back as
	 * V_ST_GC with vm==NULL; unlinking it would deref NULL and corrupt the list. */
	if(vm == NULL)
		return;
	if(var->prev != NULL)
		var->prev->next = var->next;
	else
		vm->gc.gc_vars = var->next;

	if(var->next != NULL)
		var->next->prev = var->prev;
	else
		vm->gc.gc_vars_tail = var->prev;

	var->prev = var->next = NULL;
	if(var->vm->gc.gc_vars_num > 0)
		var->vm->gc.gc_vars_num--;
}

/* Bind f->closure.func = outer AND pin outer's func_t so it outlives f.
 * closure.func is a RAW func_t* with no refcount of its own. When `outer` is a
 * transient function (an IIFE, or a callback whose last JS reference drops the
 * moment it returns), var_free() recycles outer's owner var -> func_free() frees
 * outer's func_t -> f's captured lexical chain dangles, and vm_find_in_scopes()
 * walking f->closure.func dereferences freed memory (the block reused as a string
 * such as "this"/"RegExp"/"prototype"). gc_mark's is_func walk keeps outer alive
 * across a gc SWEEP, but nothing stops a synchronous REFCOUNT release; holding a
 * var_ref on outer->owner_var here does. Released in var_free()/func_free(). */
static void func_bind_closure_func(func_t* f, func_t* outer) {
	if(f == NULL)
		return;
	f->closure.func = outer;
	/* Drop any previously pinned owner var before taking a new one (re-capture). */
	if(f->closure_func_ref != NULL) {
		var_t* old = f->closure_func_ref;
		f->closure_func_ref = NULL;
		var_unref(old);
	}
	if(outer != NULL && outer->owner_var != NULL)
		f->closure_func_ref = var_ref(outer->owner_var);
}

static bool func_set_closure(var_t* var, var_t* closure, func_t* closure_func) {
	func_t* func = var_get_func(var);
	/* Guard against re-capture: definition-time capture (vm_capture_closure)
	 * already binds the lexical scope, so the legacy return-time path must not
	 * add a second reference / extra refs-- to the same closure. */
	if(func != NULL && func->closure.var == NULL) {
		/* Capture the defining scope; do NOT touch `var`'s own refcount. This
		 * mirrors propagate_closure_cb (object-method path). The old `var->refs--`
		 * assumed a returned function always carries an extra scope-node ref, but
		 * an inline `return function(){...}` / `return () => ...` is held only by
		 * the value stack (refs==1 after handle_return's push/unref). Dropping that
		 * lone ref underflowed the uint32_t count (func_call's `ret->refs--` then
		 * wrapped it to ~4e9 and finally left the var at refs==0 on the caller's
		 * stack), so the `const f = g()` store freed and recycled it into a string
		 * (typeof f === "string"). Capturing without the decrement keeps the
		 * stack's ref intact; unreachable over-retained vars are still collected
		 * by the reachability-based gc. */
		func->closure.var = var_ref(closure);
		func_bind_closure_func(func, closure_func);
		return true;
	}
	return false;
}

static inline void gc_mark(var_t* var, bool mark);

static void gc_mark_callback(const char* key, void* value, void* user_data) {
  	bool mark = *(bool*)user_data;
  	node_t* node = (node_t*)value;
	if(!node_empty(node)) {
		node->var->gc_marked = mark;
		if(node->var->gc_marking == false) {
			gc_mark(node->var, mark);
		}
	}
}

static inline void gc_mark(var_t* var, bool mark) {
  	if(var_empty(var))
  		return;

  	var->gc_marking = true;
	var->gc_marked = mark;

  	// Use hash map iteration to mark all children
  	hash_map_iterate(&var->children, gc_mark_callback, &mark);

	/* A function holds a strong reference to the scope it was returned out of
	 * (func_set_closure), and that scope is not one of its children, so the walk
	 * above never reaches it. Without this pass a closure's captured env is
	 * swept while the function still reads and writes through it. The gc_marking
	 * test is the same cycle guard gc_mark_callback() uses - the env owns the
	 * node that holds this very function. */
	if(var->is_func) {
		func_t* func = var_get_func(var);
		if(func != NULL) {
			/* Walk the WHOLE captured lexical chain, not just the innermost env.
			 * closure.var is the defining scope's env (held by a var_ref); but
			 * closure.func is a RAW func_t* of the next-outer function with no ref
			 * and no root. Marking only closure.var let those outer func_t's be
			 * swept: func_free() recycled them and vm_find_in_scopes() then walked a
			 * dangling closure.func -> use-after-free (the freed block reused as a
			 * string, e.g. "RegExp"/"this"/"prototype"). Root each level's env AND
			 * the func_t's owner_var, which keeps that func_t alive and recursively
			 * marks its own env/chain. The chain is strictly outer-ward (acyclic),
			 * mirroring vm_find_in_scopes()'s own walk; gc_marking guards re-entry. */
			var_t* closure = func->closure.var;
			func_t* closure_func = func->closure.func;
			while(closure != NULL || closure_func != NULL) {
				if(!var_empty(closure) && closure->gc_marking == false)
					gc_mark(closure, mark);
				if(closure_func == NULL)
					break;
				var_t* fvar = closure_func->owner_var;
				if(fvar != NULL && fvar->gc_marking == false)
					gc_mark(fvar, mark);
				closure = closure_func->closure.var;
				closure_func = closure_func->closure.func;
			}
		}
	}

  	var->gc_marking = false;
}

static inline void gc_mark_cache(vm_t* vm, bool mark) {
	if(vm->var_cache.size == 0)
		return;

	uint32_t i;
	for(i=0; i<vm->var_cache.used; ++i) {
		var_t* v = vm->var_cache.cache[i];
		gc_mark(v, mark);
	}
}

static inline void gc_mark_stack(vm_t* vm, bool mark) {
	int i = vm->stack_top-1;
	while(i>=0) {
		mario_debug("mark stack\n");
		void *p = vm->stack[i];
		i--;
		if(p == NULL)
			continue;

		int8_t magic = *(int8_t*)p;
		var_t* v = NULL;
		if(magic == 0) { //var
			v = (var_t*)p;
		}
		else {//node
			node_t* node = (node_t*)p;
			if(node != NULL)
				v = node->var;
		}

		mario_debug("mark stack go %d\n", mark);
		gc_mark(v, mark);
		mario_debug("mark stack go end\n");
	}
}

/* Live scopes are gc roots too. A scope var is referenced only by its scope_t,
 * so without this pass the collector can not see it: object/array literals
 * (handle_obj) and block scopes (handle_block) build their var behind a scope
 * that is never pushed on the value stack, unlike a call env, which func_call()
 * parks there with `vm_push(vm, env); //avoid for gc`. Any var such a scope owns
 * is then swept while its node still points at it, and the next marking pass
 * dereferences freed memory. */
static inline void gc_mark_scopes(vm_t* vm, bool mark) {
	int i;
	for(i=0; i<vm->scope_stack_top; ++i) {
		scope_t* sc = vm->scope_stack[i];
		if(sc == NULL)
			continue;
		gc_mark(sc->var, mark);
		/* Root the function OBJECT var that owns sc->func (func_t). func_call()
		 * picks func_var off the value stack (or holds it as a borrowed C pointer),
		 * so during the callee's body nothing else reaches it; a gc sweep would
		 * free func_var -> func_free() frees the func_t -> sc->func dangles. Marking
		 * it also walks the func's ENTIRE captured lexical chain (gc_mark's is_func
		 * branch roots every closure.func's owner_var and env), so the outer
		 * func_t's vm_find_in_scopes() traverses stay live for this frame's whole
		 * execution. (The generator-resume frame roots its func var via the
		 * "@@gen_func" hidden member and leaves sc->func_var NULL, which gc_mark
		 * tolerates.) */
		gc_mark(sc->func_var, mark);
	}
}

void var_free(void* p) {
	var_t* var = (var_t*)p;
	if(var_empty(var))
		return;

	vm_t* vm = var->vm;
	/* Without a VM there is no free-list / gc-list to recycle into; this is
	 * recycled memory, not a live var. */
	if(vm == NULL)
		return;

	if(var->is_func) {
		func_t* func = var_get_func(var);
		if(func != NULL && (func->closure.var != NULL || func->closure_func_ref != NULL)) {
			/* Detach the closure BEFORE releasing it. A function returned out of
			 * the scope that defines it keeps two links to that same scope: the
			 * scope still owns the node holding this var, and this var's func_t
			 * owns the scope. Releasing the scope while both links are live
			 * re-enters var_free() on this very var (var_unref() frees a var whose
			 * refs are already 0), and that inner call completes the teardown -
			 * func_free()ing the func_t and queueing this var for reuse - leaving
			 * us writing through freed memory and queueing the var a second time. */
			var_t* closure = func->closure.var;
			var_t* cfunc_ref = func->closure_func_ref;
			func->closure.var = NULL;
			func->closure.func = NULL;
			func->closure_func_ref = NULL;
			/* Release the pinned owner var of the next-outer func_t first: it keeps
			 * closure.func's func_t alive, and dropping it may recursively tear down
			 * that outer closure (its own var_free releases its closure_func_ref).
			 * The lexical chain is strictly outer-ward/acyclic, so this can never
			 * re-enter teardown of THIS var. */
			if(cfunc_ref != NULL)
				var_unref(cfunc_ref);
			if(closure != NULL)
				var_unref(closure);
			//the release above may have torn this var down completely.
			if(var_empty(var))
				return;
		}
	}

	//clean var. var_clean() preserves next/prev across its memset.
	var_clean(var);
	var->type = V_UNDEF;
	var->vm = vm;

	/* Decide gc-list membership from the var's ACTUAL list pointers, never from a
	 * status snapshot taken before the closure teardown above. Releasing the closure
	 * re-enters the object graph and can var_ref()/var_unref() this very var,
	 * flipping it between the gc_vars list and the referenced set; a stale snapshot
	 * then makes us either remove_from_gc() a var that is no longer linked (which
	 * wipes gc_vars head/tail) or add_to_free() a var that IS still linked (leaving
	 * a dangling node the next gc_vars walk dereferences -> heap-use-after-free).
	 * var_clean() kept next/prev, so the true linkage is readable right here. */
	bool in_gc = (var->prev != NULL || var->next != NULL ||
		vm->gc.gc_vars == var || vm->gc.gc_vars_tail == var);

	if(in_gc) { //still linked in the gc_vars list
		if(vm->gc.is_doing_gc) { // if is doing gc, change status to GC_FREE for moving to free_var_buffer list later.
			var->status = V_ST_GC_FREE;
		}
		else { //not doing gc, move to free_var_buffer list immediately.
			remove_from_gc(var);
			add_to_free(var);
		}
	}
	else {
		add_to_free(var);
	}
}

inline var_t* var_ref(var_t* var) {
	if(var == NULL)
		return NULL;
	/* Same recycled-heap sentinel as var_unref: never touch a var with no VM. */
	if(var->vm == NULL)
		return var;
	++var->refs;
	if(var->status == V_ST_GC) {
		/*remove from vm->gc_vars list.*/
		remove_from_gc(var);
		var->status = V_ST_REF;
	}
	return var;
}

inline void var_unref(var_t* var) {
	if(var_empty(var))
		return;
	/* Recycled heap reached via a stale node->var reads back as live but has no
	 * VM; tearing it down would double-free. Treat it as already dead. */
	if(var->vm == NULL)
		return;

	if(var->refs > 0)
		--var->refs;

	if(var->refs == 0) {
		/*referenced count is 0, means this variable not be referenced anymore,
		free it immediately.*/
		var_free(var);
	}
	else if(var->status == V_ST_REF) { 
		/*referenced count not 0, means this variable still be referenced,
		add to vm->gc_vars list for rooted checking.*/
		add_to_gc(var);
	}
}

static inline void gc_vars(vm_t* vm) {
	//mario_debug("gc marking root\n");
	gc_mark(vm->root, true); //mark all rooted vars
	//mario_debug("gc marking stack\n");
	gc_mark_stack(vm, true); //mark all stacked vars
	//mario_debug("gc marking cache\n");
	gc_mark_cache(vm, true); //mark all cached vars
	gc_mark_scopes(vm, true); //mark all vars owned by a live scope
	/* builtin singletons (true/false/null) are held only by vm->builtin_vars and
	 * are NOT members of vm->root, so gc_mark(vm->root) can not reach them. Mark
	 * them explicitly, otherwise a GC triggered while a compare result is sitting
	 * in the gc_vars list (pushed then popped) would free them -> use-after-free. */
	gc_mark(vm->builtin_vars.var_true, true);
	gc_mark(vm->builtin_vars.var_false, true);
	gc_mark(vm->builtin_vars.var_null, true);
	/* Phase 6: a FinalizationRegistry cell's held value + callback are referenced
	 * only by the C-side cell (not reachable from any JS root), so mark them here or
	 * the sweep below would collect them out from under a pending finalizer. The
	 * weak TARGET is deliberately not marked - that is what makes it collectable. */
	gc_mark_weak(vm, true);

	//mario_debug("free all unmarked var\n");
	var_t* v = vm->gc.gc_vars;
	//first step: free unmarked vars
	while(v != NULL) {
		var_t* next = v->next;
		if(v->status == V_ST_GC && v->gc_marked == false) {
			if(v == vm->gc.gc_vars)
			vm->gc.gc_vars = next;
			var_free(v);
		}
		v = next;
	}

	//mario_debug("gc unmarking root\n");
	gc_mark(vm->root, false); //unmark all rooted vars
	//mario_debug("gc unmarking stack\n");
	gc_mark_stack(vm, false); //unmark all stacked vars
	//mario_debug("gc unmarking cache\n");
	gc_mark_cache(vm, false); //unmark all cached vars
	gc_mark_scopes(vm, false);
	gc_mark(vm->builtin_vars.var_true, false);
	gc_mark(vm->builtin_vars.var_false, false);
	gc_mark(vm->builtin_vars.var_null, false);
	/* Phase 6: unmark the finalizer held values. The sweep above may have moved
	 * cells from weak_cells to pending_finalizers (their targets died), so
	 * gc_mark_weak() walks both lists in both phases to keep marking symmetric. */
	gc_mark_weak(vm, false);

	//second step: move freed var to free_var_list for reusing.
	v = vm->gc.gc_vars;
	while(v != NULL) {
		var_t* next = v->next;
		if(v->status == V_ST_GC_FREE) {
			remove_from_gc(v);	
			add_to_free(v);
		}
		v = next;
	}
}

static inline void gc_free_free_vars(vm_t* vm, uint32_t buffer_num) {
	var_t* v = vm->free_var_buffer;
	while(v != NULL) {
		var_t* vtmp = v->next;
		mario_free(v);
		v = vtmp;
		vm->free_var_buffer = v;
		vm->free_var_buffer_num--;
		if(vm->free_var_buffer_num <= buffer_num)
			break;
	}
}

static inline void gc(vm_t* vm, bool force) {
	if(vm->gc.is_doing_gc)
		return;
	if(!force && vm->gc.gc_defer > 0) //a C frame holds bare var pointers right now
		return;
	if(!force && vm->gc.gc_vars_num < vm->gc.gc_trig_var_num)
		return;
	mario_debug("gc ...... ");
	vm->gc.is_doing_gc = true;
	gc_vars(vm);
	gc_free_free_vars(vm, force ? 0:vm->gc.free_var_buffer_num);
	vm->gc.is_doing_gc = false;
	/* Adaptive trigger: gc_vars_num is now the surviving live count. Require the
	 * heap to grow ~1.5x past it before the next opportunistic collection, so
	 * building a large all-live structure (a big array/object literal such as
	 * w3.org's inline membersData) costs amortized O(n) instead of re-marking
	 * the whole heap on every allocation past the fixed floor - the O(n^2) that
	 * froze the engine for seconds. The floor keeps small heaps collecting
	 * promptly, and a collection that frees a lot shrinks the trigger back. */
	{
		uint32_t live = vm->gc.gc_vars_num;
		uint32_t trig = live + (live >> 1);
		vm->gc.gc_trig_var_num = (trig < GC_TRIG_VAR_NUM_DEF) ? GC_TRIG_VAR_NUM_DEF : trig;
	}
	mario_debug("done.\n");
}

/* ====== Phase 6: weak references & finalization registry ======
 * A WeakRef observes a target object without keeping it alive; a
 * FinalizationRegistry schedules a cleanup callback to run after a target is
 * collected. Both are tracked as C-side cells keyed by the target's var pointer
 * (a cell NEVER refs the target - that is what makes the reference weak).
 *
 * Lifetime contract:
 *  - var_clean() calls vm_weak_target_dying() as a target is torn down: WeakRef
 *    cells clear their observer (value = NULL, so deref() -> undefined) and are
 *    freed; FinalizationRegistry cells move to pending_finalizers, keeping their
 *    ref'd held value + callback alive for the drain.
 *  - gc_mark_weak() marks those held values + callbacks during a collection so
 *    the sweep does not reclaim them (they are reachable only from C cells, which
 *    are not gc roots). It never marks the target.
 *  - vm_gc_collect() (the hidden gc() global) runs a forced collection then
 *    drains the pending callbacks. Draining runs JS via call_m_func(), which
 *    spins while is_doing_gc, so it MUST happen after gc() returns, never inside
 *    the sweep.
 *
 * Cells are matched by target pointer. That is safe because var_clean() clears /
 * re-queues a target's cells before var_free() recycles the var into the free
 * pool, so a pointer is never reused while a stale cell still references it. A
 * WeakRef removes its own cell via on_destroy when the WeakRef dies, and a
 * registry removes its cells the same way, so no cell outlives the observer it
 * would otherwise write through. */
typedef struct st_weak_cell {
	struct st_weak_cell* next;
	var_t*   target;    // observed var; matched by pointer, NEVER ref'd
	var_t*   weakref;   // WeakRef cell: the observing WeakRef var (value cleared on target death)
	var_t*   registry;  // FR cell: the owning FinalizationRegistry (raw ptr, scopes unregister)
	var_t*   callback;  // FR cell: ref'd cleanup function
	var_t*   held;      // FR cell: ref'd value handed to the callback
	var_t*   token;     // FR cell: unregister token (raw ptr, matched by identity); may be NULL
	bool     is_fr;     // true = FinalizationRegistry cell, false = WeakRef cell
} weak_cell_t;

static weak_cell_t* weak_cell_new(var_t* target, bool is_fr) {
	weak_cell_t* c = (weak_cell_t*)mario_malloc(sizeof(weak_cell_t));
	memset(c, 0, sizeof(weak_cell_t));
	c->target = target;
	c->is_fr = is_fr;
	return c;
}

/* Release a cell's owned references (an FR cell holds the held value + callback)
 * and free it. Never touches the target - a cell does not own it. */
static void weak_cell_free(weak_cell_t* c) {
	if(c == NULL)
		return;
	if(c->is_fr) {
		if(c->held != NULL)
			var_unref(c->held);
		if(c->callback != NULL)
			var_unref(c->callback);
	}
	mario_free(c);
}

/* Unlink one known cell from a singly-linked list head. */
static void weak_list_remove(weak_cell_t** head, weak_cell_t* c) {
	weak_cell_t** pp = head;
	while(*pp != NULL) {
		if(*pp == c) {
			*pp = c->next;
			c->next = NULL;
			return;
		}
		pp = &(*pp)->next;
	}
}

void vm_weak_add_ref(vm_t* vm, var_t* target, var_t* weakref) {
	if(vm == NULL || target == NULL || weakref == NULL)
		return;
	weak_cell_t* c = weak_cell_new(target, false);
	c->weakref = weakref;
	c->next = vm->weak_cells;
	vm->weak_cells = c;
}

/* Drop every WeakRef cell observing `weakref` (called from the WeakRef's
 * on_destroy, so a later death of the target never writes through a freed
 * WeakRef). A WeakRef cell owns no references, so removal cannot re-enter. */
void vm_weak_remove_ref(vm_t* vm, var_t* weakref) {
	if(vm == NULL || weakref == NULL)
		return;
	weak_cell_t** pp = &vm->weak_cells;
	while(*pp != NULL) {
		weak_cell_t* c = *pp;
		if(!c->is_fr && c->weakref == weakref) {
			*pp = c->next;
			c->next = NULL;
			weak_cell_free(c);
		}
		else {
			pp = &c->next;
		}
	}
}

void vm_weak_add_finalizer(vm_t* vm, var_t* registry, var_t* target, var_t* callback, var_t* held, var_t* token) {
	if(vm == NULL || target == NULL || callback == NULL)
		return;
	weak_cell_t* c = weak_cell_new(target, true);
	c->registry = registry;                 // raw ptr: only compared against a live registry
	c->callback = var_ref(callback);        // keep the cleanup alive until it runs
	c->held = (held != NULL) ? var_ref(held) : NULL;
	c->token = token;                       // raw ptr, matched by identity on unregister
	c->next = vm->weak_cells;
	vm->weak_cells = c;
}

/* unregister(token): drop this registry's cells whose token matches by identity.
 * Returns true if at least one registration was removed (spec-shaped). */
bool vm_weak_unregister(vm_t* vm, var_t* registry, var_t* token) {
	if(vm == NULL || token == NULL)
		return false;
	/* Unlink every match first, then release: weak_cell_free() unrefs the held
	 * value + callback, and a held value dropping to zero re-enters var_clean() ->
	 * vm_weak_target_dying(), which mutates weak_cells. Freeing off-list keeps that
	 * re-entrancy away from the walk. */
	weak_cell_t* doomed = NULL;
	weak_cell_t** pp = &vm->weak_cells;
	while(*pp != NULL) {
		weak_cell_t* c = *pp;
		if(c->is_fr && c->registry == registry && c->token == token) {
			*pp = c->next;
			c->next = doomed;
			doomed = c;
		}
		else {
			pp = &c->next;
		}
	}
	bool removed = (doomed != NULL);
	while(doomed != NULL) {
		weak_cell_t* n = doomed->next;
		doomed->next = NULL;
		weak_cell_free(doomed);
		doomed = n;
	}
	return removed;
}

/* A registry being collected releases all of its registrations (called from the
 * FinalizationRegistry's on_destroy). */
void vm_weak_remove_registry(vm_t* vm, var_t* registry) {
	if(vm == NULL || registry == NULL)
		return;
	weak_cell_t* doomed = NULL;   // unlink-then-free: see vm_weak_unregister
	weak_cell_t** pp = &vm->weak_cells;
	while(*pp != NULL) {
		weak_cell_t* c = *pp;
		if(c->is_fr && c->registry == registry) {
			*pp = c->next;
			c->next = doomed;
			doomed = c;
		}
		else {
			pp = &c->next;
		}
	}
	while(doomed != NULL) {
		weak_cell_t* n = doomed->next;
		doomed->next = NULL;
		weak_cell_free(doomed);
		doomed = n;
	}
}

/* var_clean() hook: the target is dying. Clear WeakRefs observing it and move FR
 * cells to the pending queue. Runs with no JS and no allocation, so it is safe
 * both inside a gc sweep and during an ordinary refcount-to-zero free. */
static void vm_weak_target_dying(vm_t* vm, var_t* target) {
	weak_cell_t** pp = &vm->weak_cells;
	while(*pp != NULL) {
		weak_cell_t* c = *pp;
		if(c->target != target) {
			pp = &c->next;
			continue;
		}
		*pp = c->next;   // unlink from the live registry
		if(c->is_fr) {
			c->next = vm->pending_finalizers;   // keep held + callback for the drain
			vm->pending_finalizers = c;
		}
		else {
			if(c->weakref != NULL)
				c->weakref->value = NULL;        // cleared: deref() now returns undefined
			c->next = NULL;
			weak_cell_free(c);                   // a WeakRef cell owns no refs
		}
	}
}

/* gc_vars() hook: shield the values a registered/pending finalizer still needs.
 * Walks both lists because a sweep moves cells from weak_cells to
 * pending_finalizers between the mark and unmark passes. */
static void gc_mark_weak(vm_t* vm, bool mark) {
	weak_cell_t* c;
	for(c = vm->weak_cells; c != NULL; c = c->next) {
		if(c->is_fr) {
			if(c->held != NULL) gc_mark(c->held, mark);
			if(c->callback != NULL) gc_mark(c->callback, mark);
		}
	}
	for(c = vm->pending_finalizers; c != NULL; c = c->next) {
		if(c->held != NULL) gc_mark(c->held, mark);
		if(c->callback != NULL) gc_mark(c->callback, mark);
	}
}

/* Run the queued cleanup callbacks. Called only from vm_gc_collect() AFTER
 * gc(vm,true) has returned, so is_doing_gc is false and call_m_func() (which
 * spins on it) is safe. A cell stays linked in pending_finalizers while its own
 * callback runs, so a nested forced collection still marks its held value +
 * callback; it is unlinked afterwards (a nested finalizer may have pushed new
 * cells ahead of it). finalizers_draining guards against a callback that calls
 * gc() re-entering the drain and double-firing. */
static void vm_drain_finalizers(vm_t* vm) {
	if(vm->pending_finalizers == NULL || vm->finalizers_draining)
		return;
	vm->finalizers_draining = true;
	vm->gc.gc_defer++;   // args + cells are unrooted C locals across each callback
	while(vm->pending_finalizers != NULL) {
		weak_cell_t* c = vm->pending_finalizers;   // leave linked: gc_mark_weak shields it
		if(c->callback != NULL) {
			var_t* args = var_new_array(vm);
			var_array_add(args, (c->held != NULL) ? c->held : var_new(vm));
			var_array_reverse(args);   // call_m_func expects the last arg at index 0
			var_t* res = call_m_func(vm, NULL, c->callback, args);
			if(res != NULL)
				var_unref(res);
			var_unref(args);
		}
		weak_list_remove(&vm->pending_finalizers, c);
		weak_cell_free(c);
	}
	vm->gc.gc_defer--;
	vm->finalizers_draining = false;
}

/* The hidden gc() global: force a full collection (which clears dead WeakRefs and
 * queues finalizers) and then drain the queue at this safe point. */
void vm_gc_collect(vm_t* vm) {
	if(vm == NULL)
		return;
	gc(vm, true);
	vm_drain_finalizers(vm);
}

/* vm_close() hook: release every remaining cell without running callbacks (the VM
 * is shutting down). Frees the FR held values + callbacks so teardown stays clean
 * under a leak checker. */
static void weak_registry_free(vm_t* vm) {
	/* Detach both lists up front so a held value dropping to zero during a
	 * weak_cell_free() re-enters var_clean() -> vm_weak_target_dying() against an
	 * empty registry (the var_clean gate tests vm->weak_cells != NULL) instead of
	 * walking the list we are still freeing. */
	weak_cell_t* cells = vm->weak_cells;
	weak_cell_t* pending = vm->pending_finalizers;
	vm->weak_cells = NULL;
	vm->pending_finalizers = NULL;
	weak_cell_t* c = cells;
	while(c != NULL) {
		weak_cell_t* n = c->next;
		c->next = NULL;
		weak_cell_free(c);
		c = n;
	}
	c = pending;
	while(c != NULL) {
		weak_cell_t* n = c->next;
		c->next = NULL;
		weak_cell_free(c);
		c = n;
	}
}

static const char* get_typeof(var_t* var) {
	switch(var->type) {
		case V_UNDEF:
			return "undefined";
		case V_INT:
		case V_FLOAT:
		case V_INT64:
		case V_FLOAT64:
			return "number";
		case V_BIGINT:
			return "bigint";
		case V_BOOL: 
			return "boolean";
		case V_STRING: 
			return "string";
		case V_NULL: 
			/* Historic JS quirk: typeof null === "object" (a bug preserved for
			 * backwards compatibility since the very first JS engine). */
			return "object";
		case V_OBJECT: 
			if(var_is_symbol(var))
				return "symbol";
			return (var->is_func || var->is_class) ? "function": "object";
	}
	return "undefined";
}

var_t* var_get_prototype(var_t* var) {
	return get_obj(var, PROTOTYPE);
}

void var_set_prototype(var_t* var, var_t* proto) {
	if(var == NULL || proto == NULL)
		return;
	node_t* ret = var_add(var, PROTOTYPE, proto);
	ret->invisable = 1;
	//ret->be_inherited = 1;
	ret->be_unenumerable = 1;
}

inline var_t* var_new(vm_t* vm) {
	var_t* var = get_from_free(vm);
	if(var == NULL) {
        uint32_t sz = sizeof(var_t);
		var = (var_t*)mario_malloc(sz);
	}

	memset(var, 0, sizeof(var_t));
	var->type = V_UNDEF;
	var->vm = vm;
	var->status = V_ST_REF;
	// Initialize hash map for children
	hash_map_init(&var->children);
	return var;
}

inline var_t* var_new_block(vm_t* vm) {
	var_t* var = var_new_obj_no_proto(vm, NULL, NULL);
	return var;
}

inline var_t* var_new_array(vm_t* vm) {
	var_t* var = var_new_obj(vm, var_get_prototype(vm->builtin_vars.var_Array), NULL, NULL);
	var->is_array = 1;
	var_t* members = var_new_obj_no_proto(vm, NULL, NULL);
	node_t* n = var_add(var, "_ARRAY_", members);
	n->be_unenumerable = 1;
	n->invisable = 1;
	return var;
}

inline var_t* var_new_int(vm_t* vm, int i) {
	var_t* var = var_new(vm);
	var->type = V_INT;
	var->value = mario_malloc(sizeof(int));
	*((int*)var->value) = i;
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_Number));
	return var;
}

inline var_t* var_new_int64(vm_t* vm, int64_t i) {
	var_t* var = var_new(vm);
	var->type = V_INT64;
	var->value = mario_malloc(sizeof(int64_t));
	*((int64_t*)var->value) = i;
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_Number));
	return var;
}

inline var_t* var_new_null(vm_t* vm) {
	var_t* var = var_new(vm);
	var->type = V_NULL;
	return var;
}

inline var_t* var_new_bool(vm_t* vm, bool b) {
	var_t* var = var_new(vm);
	var->type = V_BOOL;
	var->value = mario_malloc(sizeof(int));
	*((int*)var->value) = b;
	return var;
}

inline var_t* var_new_obj_no_proto(vm_t* vm, void*p, free_func_t fr) {
	var_t* var = var_new(vm);
	var->type = V_OBJECT;
	var->value = p;
	var->free_func = fr;
	return var;
}

inline var_t* var_new_obj(vm_t* vm, var_t* proto, void*p, free_func_t fr) {
	var_t* var = var_new_obj_no_proto(vm, p, fr);
	var_set_prototype(var, proto);
	return var;
}

inline var_t* var_new_float(vm_t* vm, float i) {
	var_t* var = var_new(vm);
	var->type = V_FLOAT;
	var->value = mario_malloc(sizeof(float));
	*((float*)var->value) = i;
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_Number));
	return var;
}

inline var_t* var_new_float64(vm_t* vm, double d) {
	var_t* var = var_new(vm);
	var->type = V_FLOAT64;
	var->value = mario_malloc(sizeof(double));
	*((double*)var->value) = d;
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_Number));
	return var;
}

/* Wrap a bignum_t (ownership transfers to the var; freed via bn_free). The
 * prototype is BigInt.prototype so `(5n).toString(2)` resolves; guarded because
 * var_BigInt is only cached after the natives are registered. */
inline var_t* var_new_bigint(vm_t* vm, bignum_t* b) {
	var_t* var = var_new(vm);
	var->type = V_BIGINT;
	var->value = b;
	var->free_func = bn_free;
	var_t* proto = (vm->builtin_vars.var_BigInt != NULL)
	               ? var_get_prototype(vm->builtin_vars.var_BigInt) : NULL;
	var_set_prototype(var, proto);
	return var;
}

inline bignum_t* var_get_bigint(var_t* var) {
	if(var == NULL || var->type != V_BIGINT)
		return NULL;
	return (bignum_t*)var->value;
}

inline var_t* var_new_str(vm_t* vm, const char* s) {
	var_t* var = var_new(vm);
	var->type = V_STRING;
	var->size = (uint32_t)strlen(s);
	var->value = mario_malloc(var->size + 1);
	memcpy(var->value, s, var->size + 1);
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_String));
	return var;
}

inline var_t* var_new_str2(vm_t* vm, const char* s, uint32_t len) {
	var_t* var = var_new(vm);
	var->type = V_STRING;
	var->size = (uint32_t)strlen(s);
	if(var->size > len)
		var->size = len;
	var->value = mario_malloc(var->size + 1);
	memcpy(var->value, s, var->size + 1);
	((char*)(var->value))[var->size] = 0;
	var_set_prototype(var, var_get_prototype(vm->builtin_vars.var_String));
	return var;
}

inline const char* var_get_str(var_t* var) {
	if(var == NULL || var->value == NULL)
		return "";
	
	return (const char*)var->value;
}

inline var_t* var_set_str(var_t* var, const char* v) {
	if(v == NULL)
		return var;

	var->type = V_STRING;
	if(var->value != NULL)
		mario_free(var->value);
	uint32_t len = (uint32_t)strlen(v)+1;
	var->value = mario_malloc(len);
	memcpy(var->value, v, len);
	return var;
}

inline bool var_get_bool(var_t* var) {
	if(var == NULL || var->value == NULL)
		return false;
	switch(var->type) {
		case V_INT64:   return *(int64_t*)var->value != 0;
		case V_FLOAT:   return *(float*)var->value != 0.0f;
		case V_FLOAT64: return *(double*)var->value != 0.0;
		case V_BIGINT:  return ((bignum_t*)var->value)->sign != 0;
		default:        return *(int*)var->value != 0; // V_BOOL / V_INT
	}
}

/* JS ToBoolean for the logical-assignment operators (`||= &&=`) and any place
 * that needs real truthiness rather than the raw int-slot test var_get_bool()
 * does. Empty string / 0 / NaN / null / undefined / false are falsy; every
 * object (array, function) is truthy. */
static inline bool var_truthy(var_t* v) {
	if(v == NULL || v->value == NULL) {
		// null/undefined carry no value buffer; a live object always has one.
		return (v != NULL && v->type == V_OBJECT);
	}
	switch(v->type) {
		case V_UNDEF:
		case V_NULL:
			return false;
		case V_BOOL:
		case V_INT:
			return *(int*)v->value != 0;
		case V_INT64:
			return *(int64_t*)v->value != 0;
		case V_FLOAT: {
			float f = *(float*)v->value;
			return f != 0.0f && f == f; // NaN is falsy
		}
		case V_FLOAT64: {
			double d = *(double*)v->value;
			return d != 0.0 && d == d; // NaN is falsy
		}
		case V_BIGINT:
			return ((bignum_t*)v->value)->sign != 0; // 0n is falsy
		case V_STRING:
			return var_get_str(v)[0] != 0;
		default: // V_OBJECT and friends
			return true;
	}
}

static inline bool var_is_nullish(var_t* v) {
	return (v == NULL || v->type == V_NULL || v->type == V_UNDEF);
}

inline int var_get_int(var_t* var) {
	if(var == NULL || var->value == NULL)
		return 0;
	switch(var->type) {
		case V_FLOAT:   return (int)(*(float*)var->value);
		case V_INT64:   return (int)(*(int64_t*)var->value);
		case V_FLOAT64: return (int)(*(double*)var->value);
		case V_BIGINT:  return (int)bn_to_int64((bignum_t*)var->value);
		default:        return *(int*)var->value; // V_INT / V_BOOL
	}
}

inline var_t* var_set_int(var_t* var, int v) {
	var->type = V_INT;
	if(var->value != NULL)
		mario_free(var->value);
	var->value = mario_malloc(sizeof(int));
	*((int*)var->value) = v;
	return var;
}

inline float var_get_float(var_t* var) {
	if(var == NULL || var->value == NULL)
		return 0.0;
	switch(var->type) {
		case V_INT:     return (float)(*(int*)var->value);
		case V_INT64:   return (float)(*(int64_t*)var->value);
		case V_FLOAT64: return (float)(*(double*)var->value);
		case V_BIGINT:  return (float)bn_to_double((bignum_t*)var->value);
		default:        return *(float*)var->value; // V_FLOAT
	}
}

inline var_t* var_set_float(var_t* var, float v) {
	var->type = V_FLOAT;
	if(var->value != NULL)
		mario_free(var->value);
	var->value = mario_malloc(sizeof(float));
	*((float*)var->value) = v;
	return var;
}

inline int64_t var_get_int64(var_t* var) {
	if(var == NULL || var->value == NULL)
		return 0;
	switch(var->type) {
		case V_INT:     return (int64_t)(*(int*)var->value);
		case V_INT64:   return *(int64_t*)var->value;
		case V_FLOAT:   return (int64_t)(*(float*)var->value);
		case V_FLOAT64: return (int64_t)(*(double*)var->value);
		case V_BIGINT:  return bn_to_int64((bignum_t*)var->value);
		case V_BOOL:    return (int64_t)(*(int*)var->value);
		default:        return 0;
	}
}

inline var_t* var_set_int64(var_t* var, int64_t v) {
	var->type = V_INT64;
	if(var->value != NULL)
		mario_free(var->value);
	var->value = mario_malloc(sizeof(int64_t));
	*((int64_t*)var->value) = v;
	return var;
}

inline double var_get_float64(var_t* var) {
	if(var == NULL || var->value == NULL)
		return 0.0;
	switch(var->type) {
		case V_INT:     return (double)(*(int*)var->value);
		case V_INT64:   return (double)(*(int64_t*)var->value);
		case V_FLOAT:   return (double)(*(float*)var->value);
		case V_FLOAT64: return *(double*)var->value;
		case V_BIGINT:  return bn_to_double((bignum_t*)var->value);
		case V_BOOL:    return (double)(*(int*)var->value);
		default:        return 0.0;
	}
}

inline var_t* var_set_float64(var_t* var, double v) {
	var->type = V_FLOAT64;
	if(var->value != NULL)
		mario_free(var->value);
	var->value = mario_malloc(sizeof(double));
	*((double*)var->value) = v;
	return var;
}

/* True for every numeric tag (V_INT/V_INT64/V_FLOAT/V_FLOAT64). Used by the
 * comparison and arithmetic paths so all four interoperate as JS numbers. */
inline bool var_is_number(var_t* var) {
	if(var == NULL)
		return false;
	return var->type == V_INT || var->type == V_INT64 ||
	       var->type == V_FLOAT || var->type == V_FLOAT64;
}

inline func_t* var_get_func(var_t* var) {
	if(var == NULL || var->value == NULL || !var->is_func)
		return NULL;
	
	return (func_t*)var->value;
}

static void get_m_str(const char* str, mstr_t* ret) {
	mstr_reset(ret);
	mstr_add(ret, '"');
	/*
	while(*str != 0) {
		switch (*str) {
			case '\\': mstr_append(ret, "\\\\"); break;
			case '\n': mstr_append(ret, "\\n"); break;
			case '\r': mstr_append(ret, "\\r"); break;
			case '\a': mstr_append(ret, "\\a"); break;
			case '"':  mstr_append(ret, "\\\""); break;
			default: mstr_add(ret, *str);
		}
		str++;
	}
	*/
	mstr_append(ret, str);
	mstr_add(ret, '"');
}

/* ES6 Symbol.toPrimitive: if obj defines [Symbol.toPrimitive], call it with the
 * given hint ("number"/"string"/"default") and return its primitive result, or
 * NULL when absent or non-primitive (caller then falls back to the default
 * coercion path). */
static var_t* vm_to_primitive(vm_t* vm, var_t* var, const char* hint) {
	if(vm == NULL || var == NULL || var->type != V_OBJECT)
		return NULL;
	node_t* tp = var_find_member(var, SYMKEY_TOPRIMITIVE);
	if(tp == NULL || tp->var == NULL || !tp->var->is_func)
		return NULL;
	var_t* args = var_new_array(vm);
	var_array_add(args, var_new_str(vm, hint));
	var_array_reverse(args);
	var_t* r = call_m_func(vm, var, tp->var, args);
	var_unref(args);
	if(r != NULL && (r->type == V_OBJECT || r->type == V_UNDEF)) {
		var_unref(r);
		return NULL;
	}
	return r;
}

void var_to_str(var_t* var, mstr_t* ret) {
	mstr_reset(ret);
	if(var == NULL) {
		mstr_cpy(ret, "undefined");
		return;
	}

	switch(var->type) {
	case V_INT:
		mstr_cpy(ret, mstr_from_int(var_get_int(var), 10));
		break;
	case V_INT64:
		mstr_cpy(ret, mstr_from_int64(var_get_int64(var), 10));
		break;
	case V_FLOAT:
		mstr_cpy(ret, mstr_from_float(var_get_float(var)));
		break;
	case V_FLOAT64:
		mstr_cpy(ret, mstr_from_float64(var_get_float64(var)));
		break;
	case V_STRING:
		mstr_cpy(ret, var_get_str(var));
		break;
	case V_OBJECT:
		/* JS ToPrimitive(string): an object is stringified by calling its
		 * toString() (own or inherited) when that yields a primitive; otherwise
		 * fall back to the JSON form. Functions keep the JSON `function () {}`
		 * rendering. A depth cap stops a toString() that re-enters string
		 * conversion from recursing without bound. JSON.stringify is unaffected:
		 * it calls var_to_json_str() directly, which serialises objects itself. */
		{
			vm_t* vm = var->vm;
			var_t* rval = NULL;
			if(vm != NULL && !var->is_func && vm->to_str_depth < 16) {
				/* Symbol.toPrimitive takes precedence over toString. */
				if(!var_is_symbol(var))
					rval = vm_to_primitive(vm, var, "string");
				if(rval == NULL) {
					node_t* ts = var_find_member(var, "toString");
					if(ts != NULL && ts->var != NULL && ts->var->is_func) {
						vm->to_str_depth++;
						rval = call_m_func(vm, var, ts->var, NULL);
						vm->to_str_depth--;
					}
				}
			}
			if(rval != NULL && rval->type != V_OBJECT && rval->type != V_UNDEF) {
				var_to_str(rval, ret);
				var_unref(rval);
			}
			else {
				if(rval != NULL)
					var_unref(rval);
				var_to_json_str(var, ret, 0, false);
			}
		}
		break;
	case V_BOOL:
		mstr_cpy(ret, var_get_int(var) == 1 ? "true":"false");
		break;
	case V_NULL:
		mstr_cpy(ret, "null");
		break;
	case V_BIGINT:
		bn_to_mstr((bignum_t*)var->value, 10, ret);
		break;
	default:
		mstr_cpy(ret, "undefined");
		break;
	}
}

static void get_parsable_str(var_t* var, mstr_t* ret) {
	mstr_reset(ret);

	mstr_t* s = mstr_new("");
	var_to_str(var, s);
	if(var->type == V_STRING)
		get_m_str(s->cstr, ret);
	else
		mstr_cpy(ret, s->cstr);

	mstr_free(s);
}

static void append_json_spaces(mstr_t* ret, int level) {
	int spaces;
	for (spaces = 0; spaces<=level; ++spaces) {
        mstr_add(ret, ' '); mstr_add(ret, ' ');
	}
}

static bool _done_arr_inited = false;
/* `compact` selects standard JSON.stringify formatting (no whitespace) versus the
 * indented form used for console/debug object rendering. `level` still drives the
 * cycle-detection reset (level==0) and, when not compact, the indentation depth. */
void var_to_json_str(var_t* var, mstr_t* ret, int level, bool compact) {
	mstr_reset(ret);

	uint32_t i;

	//check if done to avoid dead recursion
	static m_array_t done;
	if(level == 0) {
		if(!_done_arr_inited) {		
			array_init(&done);
			_done_arr_inited = true;
		}
		array_remove_all(&done);
	}
	if(var->type == V_OBJECT) {
		uint32_t sz = done.size;
		for(i=0; i<sz; ++i) {
			if(done.items[i] == var) { //already done before.
				mstr_cpy(ret, "{}");
				if(level == 0)
					array_remove_all(&done);
				return;
			}
		}
		array_add(&done, var);
	}

	if (var->is_array) {
		mstr_add(ret, '[');
		uint32_t len = var_array_size(var);
		if (len>100) len=100; // we don't want to get stuck here!

		uint32_t i;
		for (i=0;i<len;i++) {
			node_t* n = var_array_get(var, i);

			mstr_t* s = mstr_new("");
			var_to_json_str(n->var, s, level, compact);
			mstr_append(ret, s->cstr);
			mstr_free(s);

			if (i<len-1) 
				mstr_append(ret, compact ? "," : ", ");
		}
		mstr_add(ret, ']');
	}
	else if (var->is_func) {
		mstr_append(ret, "function (");
		// get list of parameters
		int sz = 0;
		if(var->value != NULL) {
			func_t* func = var_get_func(var);
			sz = func->args.size;
			int i=0;
			for(i=0; i<sz; ++i) {
				mstr_append(ret, (const char*)func->args.items[i]);
				if ((i+1) < sz) {
					mstr_append(ret, ", ");
				}
			}
		}
		// add function body
		mstr_append(ret, ") {}");
		//return;
	}
	else if (var->type == V_OBJECT) {
		// children - handle with bracketed list
		if(compact)
			mstr_add(ret, '{');
		else
			mstr_append(ret, "{\n");

		// 直接遍历 hash map，不使用回调函数
		bool first = true;
		uint32_t i;
		for (i = 0; i < var->children.capacity; i++) {
			hash_entry_t* entry = var->children.buckets[i];
			while (entry) {
				const char* key = entry->key;
				node_t* node = (node_t*)entry->value;

				// 跳过不可枚举的属性
				if (node->be_unenumerable)
					goto next_entry;

				// 跳过内部属性（如原型链）
				if (strcmp(key, PROTOTYPE) == 0 || strcmp(key, "_ARRAY_") == 0)
					goto next_entry;

				// 添加逗号分隔符
				if (!first) {
					mstr_append(ret, compact ? "," : ",\n");
				} else {
					first = false;
				}

				// 缩进
				if(!compact)
					append_json_spaces(ret, level);

				// 添加属性名
				mstr_add(ret, '"');
				mstr_append(ret, key);
				mstr_add(ret, '"');
				mstr_append(ret, compact ? ":" : ": ");

				// 序列化属性值
				mstr_t* value_str = mstr_new("");
				var_to_json_str(node->var, value_str, level + 1, compact);
				mstr_append(ret, value_str->cstr);
				mstr_free(value_str);

			next_entry:
				entry = entry->next;
			}
		}

		// 如果没有属性，确保格式正确
		if (!compact) {
			if (first) {
				append_json_spaces(ret, level);
			} else {
				mstr_add(ret, '\n');
				append_json_spaces(ret, level - 1);
			}
		}

		mstr_add(ret, '}');
	} 
	else {
		// no children or a function... just write value directly
		mstr_t* s = mstr_new("");
		get_parsable_str(var, s);
		mstr_append(ret, s->cstr);
		mstr_free(s);
	}

	if(level == 0) {
		array_remove_all(&done);
	}
}

/**======Interrupt functions======*/

inline void vm_push(vm_t* vm, var_t* var) {  
	if(var == NULL)
		return;
	var_ref(var);
	if(vm->stack_top < VM_STACK_MAX) {
		vm->stack[vm->stack_top++] = var; 
	} 
}

inline void vm_push_node(vm_t* vm, node_t* node) {
	var_ref(node->var); 
	if(vm->stack_top < VM_STACK_MAX)
		vm->stack[vm->stack_top++] = node;
}

var_t* vm_pop2(vm_t* vm) {
	if(vm->stack_top == 0)
		return NULL;
	void *p = NULL;
	vm->stack_top--;
	p = vm->stack[vm->stack_top];
	int8_t magic = *(int8_t*)p;
	var_t* v = NULL;
	//var
	if(magic == 0) {
		v = (var_t*)p;
	}
	else {
		//node
		node_t* node = (node_t*)p;
		if(!node_empty(node)) {
			v = node->var;
		}
	}

	if(var_empty(v))
		return NULL;
	return v;
}

/*#define vm_pop2(vm) ({ \
	(vm)->stack_top--; \
	void* p = (vm)->stack[(vm)->stack_top]; \
	int8_t magic = *(int8_t*)p; \
	var_t* ret = NULL; \
	if(magic == 0) { \
		ret = (var_t*)p; \
	} \
	else { \
	node_t* node = (node_t*)p; \
	if(node != NULL) \
		ret = node->var; \
	else \
		ret = NULL; \
	} \
	ret; \
})
*/

bool vm_pop(vm_t* vm) {
	if(vm->stack_top == 0)
		return false;

	vm->stack_top--;
	void *p = vm->stack[vm->stack_top];
	int8_t magic = *(int8_t*)p;
	var_t* v = NULL;
	if(magic == 0) { //var
		v = (var_t*)p;
	}
	else { //node
		node_t* node = (node_t*)p;
		if(!node_empty(node)) {
			v = node->var;
		}
	}

	if(!var_empty(v))
		var_unref(v);
	return true;
}

/* Peek the top of the stack as a node WITHOUT popping it. vm_pop2() unwraps a
 * node to its var and throws the binding away, but ++/-- and the compound
 * assignments need the node to write a fresh value back through it. Returns
 * NULL when the top entry is a plain var (an rvalue) - nothing to assign to. */
static inline node_t* vm_peek_node(vm_t* vm) {
	if(vm->stack_top == 0)
		return NULL;
	void* p = vm->stack[vm->stack_top-1];
	if(p == NULL)
		return NULL;
	if(*(int8_t*)p != 1) //not a node!
		return NULL;
	return (node_t*)p;
}

/* Peek the top of the stack as a var WITHOUT popping it, whether the entry is a
 * plain var (rvalue) or a node (lvalue binding). Used by `??`/`?.` to inspect a
 * value while leaving it in place for a possible short-circuit. */
static inline var_t* vm_peek_var(vm_t* vm) {
	if(vm->stack_top == 0)
		return NULL;
	void* p = vm->stack[vm->stack_top-1];
	if(p == NULL)
		return NULL;
	if(*(int8_t*)p == 0) //plain var
		return (var_t*)p;
	node_t* node = (node_t*)p;
	if(node_empty(node))
		return NULL;
	return node->var;
}

static inline node_t* vm_pop2node(vm_t* vm) {
	if(vm->stack_top == 0)
		return NULL;
	void *p = NULL;
	vm->stack_top--;
	p = vm->stack[vm->stack_top];
	int8_t magic = *(int8_t*)p;
	if(magic != 1) {//not node!
		return NULL;
	}

	return (node_t*)p;
}

static var_t* vm_stack_pick(vm_t* vm, int depth) {
	int index = vm->stack_top - depth;
	if(index < 0)
		return NULL;

	void* p = vm->stack[index];
	var_t* ret = NULL;
	if(p == NULL)
		return ret;

	int8_t magic = *(int8_t*)p;
	if(magic == 1) {//node
		node_t* node = (node_t*)p;
		if(node != NULL)
			ret = node->var;
	}
	else {
		ret = (var_t*)p;
	}

	vm->stack_top--;
	int i;
	for(i=index; i<vm->stack_top; ++i) {
		vm->stack[i] = vm->stack[i+1];
	}
	return ret;
}

#define vm_get_scope(vm) ((vm)->scope_stack_top > 0 ? (vm)->scope_stack[(vm)->scope_stack_top - 1] : NULL)

static inline var_t* vm_get_scope_var(vm_t* vm) {
	var_t* ret = vm->root;
	scope_t* sc = vm_get_scope(vm);
	if(sc != NULL && !var_empty(sc->var))
		ret = sc->var;
	return ret;
}

static scope_t* scope_new(var_t* var) {
	scope_t* sc = (scope_t*)mario_malloc(sizeof(scope_t));
	memset(sc, 0, sizeof(scope_t));

	if(var != NULL)
		sc->var = var_ref(var);
	sc->pc = 0;
	sc->pc_start = 0;
	sc->is_block = false;
	sc->is_func = false;
	sc->is_try = false;
	sc->is_loop = false;
	sc->is_switch = false;
	return sc;
}

/*static scope_t* scope_clone(scope_t* src) {
	scope_t* sc = (scope_t*)mario_malloc(sizeof(scope_t));
	memcpy(sc, src, sizeof(scope_t));
	if(src->var != NULL)
		sc->var = var_ref(src->var);
	return sc;
}
*/

static void scope_free(void* p) {
	scope_t* sc = (scope_t*)p;
	if(sc == NULL)
		return;
	if(sc->var != NULL)
		var_unref(sc->var);
	mario_free(sc);
}
/*#define vm_get_scope_var(vm, skipBlock) ({ \
	scope_t* sc = (scope_t*)array_tail((vm)->scopes); \
	if((skipBlock) && sc != NULL && sc->var->type == V_BLOCK) \
		sc = sc->prev; \
	var_t* ret; \
	if(sc == NULL) \
		ret = (vm)->root; \
	else \
		ret = sc->var; \
	ret; \
})
*/

static void vm_push_scope(vm_t* vm, scope_t* sc) {
	scope_t* prev = NULL;
	if(vm->scope_stack_top > 0) {
		prev = vm->scope_stack[vm->scope_stack_top - 1];
	}
	if(vm->scope_stack_top < VM_SCOPE_STACK_MAX) {
		vm->scope_stack[vm->scope_stack_top] = sc;
		vm->scope_stack_top++;
		sc->prev = prev;
	}
}

static PC vm_pop_scope(vm_t* vm) {
	if(vm->scope_stack_top <= 0)
		return 0;

	PC pc = 0;
	scope_t* sc = vm_get_scope(vm);
	if(sc == NULL)
		return 0;

	if(sc->is_func)
		pc = sc->pc;
	vm->scope_stack_top--;
	if(sc->var != NULL && sc->var->refs > 1) //captured by a closure: outlives this scope
		load_ncache_invalidate_var(vm, sc->var);
	scope_free(sc);
	vm->gc.gc_pending = true; /* defer to vm_run safe point */
	return pc;
}

node_t* vm_find(vm_t* vm, const char* name) {
	var_t* var = vm_get_scope_var(vm);
	if(var_empty(var))
		return NULL;
	return var_find_own_member(var, name);	
}

node_t* vm_find_in_class(var_t* var, const char* name) {
	var_t* proto = var_get_prototype(var);
	while(proto != NULL) {
		node_t* ret = NULL;
		ret = var_find_own_member(proto, name);
		if(ret != NULL) {
			if(ret->var != NULL && ret->var->is_func)
				return ret;
			ret = var_add(var, name, var_clone(ret->var));
			ret->be_inherited = 1;
			return ret;
		}
		proto = var_get_prototype(proto);
	}
	return NULL;
}

bool var_instanceof(var_t* var, var_t* proto) {
	/* `instanceof` only applies to objects. A primitive (number, string, boolean,
	 * null, undefined) is never an instance of anything, even though mario gives
	 * primitives a builtin prototype so methods like (1).toFixed / "x".charAt can
	 * be dispatched. Without this guard `1 instanceof Number` wrongly walked into
	 * Number.prototype and returned true. */
	if(var == NULL || var->type != V_OBJECT)
		return false;

	var_t* v = var_get_prototype(proto);
	if(v != NULL)
		proto = v;
		
	var_t* protov = var_get_prototype(var);
	while(protov != NULL) {
		if(protov == proto)
			return true;
		protov = var_get_prototype(protov);
	}
	return false;
}

static inline node_t* vm_find_in_scopes(vm_t* vm, const char* name) {
	node_t* ret = NULL;
	scope_t* sc = vm_get_scope(vm);
	if(sc != NULL && sc->is_func) {
		var_t* closure = sc->func->closure.var;
		func_t* closure_func = sc->func->closure.func;
		while(closure != NULL) {
			ret = var_find_own_member(closure, name);	
			if(ret != NULL)
				return ret;
			
			/* A block-captured closure (handle_func) links each block var to its
			 * lexical parent via the hidden "@@lex" member; climb it before falling
			 * back to the next function's closure. */
			node_t* lex = var_find_own_member(closure, "@@lex");
			if(lex != NULL && !var_empty(lex->var)) {
				closure = lex->var;
				continue;
			}
			if(closure_func == NULL)
				break;
			closure = closure_func->closure.var;
			closure_func = closure_func->closure.func;
		}
	}
	
	bool closure_done = (sc != NULL && sc->is_func); /* already walked above */
	while(sc != NULL) {
		if(!var_empty(sc->var)) {
			ret = var_find_own_member(sc->var, name);
			if(ret != NULL)
				return ret;
			
			var_t* obj = get_obj(sc->var, THIS);
			if(obj != NULL) {
				ret = var_find_member(obj, name);
				if(ret != NULL)
					return ret;
			}
		}
		/* Nested block / object-literal scopes are not is_func, so their captured
		 * variables live on the nearest enclosing function's closure. When the walk
		 * reaches that function scope, search its closure chain (the definition
		 * scope) before following prev into the caller's scopes; otherwise a missing
		 * name would be auto-created in the wrong (e.g. object-literal) scope. */
		if(sc->is_func && !closure_done) {
			closure_done = true;
			var_t* closure = (sc->func != NULL) ? sc->func->closure.var : NULL;
			func_t* closure_func = (sc->func != NULL) ? sc->func->closure.func : NULL;
			while(closure != NULL) {
				ret = var_find_own_member(closure, name);
				if(ret != NULL)
					return ret;
				node_t* lex = var_find_own_member(closure, "@@lex");
				if(lex != NULL && !var_empty(lex->var)) {
					closure = lex->var;
					continue;
				}
				if(closure_func == NULL)
					break;
				closure = closure_func->closure.var;
				closure_func = closure_func->closure.func;
			}
		}
		sc = sc->prev;
	}

	return var_find_own_member(vm->root, name);
}

static inline scope_t* vm_get_try_catch_scope(vm_t* vm) {
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL) {
		if(sc->is_try)
			return sc;
		sc = sc->prev;
	}
	return NULL;
}

/* A runtime throw abandons the interrupted expression, but its transient
 * value-stack entries stay put: e.g. `let x = <expr>` pushes the binding node
 * (INSTR_LOAD) before <expr> is evaluated, so a throw inside <expr> leaves that
 * node on the value stack. The unwind then frees the scope that owns the node
 * (scope_free -> var_free -> var_remove_all), and a later func_call vm_pop2() /
 * vm_pop() dereferences the freed node -> heap-use-after-free / SIGSEGV. It also
 * leaks ~2 slots per caught throw, overflowing VM_STACK_MAX after ~14 throws.
 * Drop every entry above the innermost function frame's entry baseline except
 * the thrown value on top (which handle_catch() pops). This mirrors the native
 * throw path, where func_call pops its env before unwinding, so the suspended
 * func_call frames still find their own env intact. */
static void vm_throw_truncate(vm_t* vm) {
	if(vm->stack_top <= 0)
		return;
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL && !sc->is_func)
		sc = sc->prev;
	int32_t base = (sc != NULL) ? sc->stack_top : 0;
	if(base < 0)
		base = 0;
	if(vm->stack_top <= base + 1)
		return; // only the thrown value (or nothing) sits above the frame baseline
	void* err = vm->stack[vm->stack_top - 1]; // the thrown value, held aside (its ref is untouched)
	vm->stack_top--;
	while(vm->stack_top > base)
		vm_pop(vm); // unref each leaked operand/binding while its owning scope is still alive
	vm->stack[vm->stack_top++] = err; // re-seat the thrown value on top
}

static inline scope_t* vm_get_strict_scope(vm_t* vm) {
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL) {
		if(sc->is_strict)
			return sc;
		sc = sc->prev;
	}
	return NULL;
}

static inline scope_t* vm_get_loop_scope(vm_t* vm) {
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL) {
		if(sc->is_loop)
			return sc;
		sc = sc->prev;
	}
	return NULL;
}

void vm_throw(vm_t* vm, const char *format, ...) {
	char message[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, BUF_SIZE, format, ap);
	va_end(ap);

	var_t* err = var_new_obj(vm, vm->builtin_vars.var_Error, NULL, NULL);
	var_t* msg = var_find_member_var(err, "message");
	if(msg != NULL)
		var_set_str(msg, message);
	vm_push(vm, err);

	scope_t* try_sc = vm_get_try_catch_scope(vm);
	if(try_sc == NULL) {
		vm_pop(vm);
		return;
	}

	vm_throw_truncate(vm); // drop operands the interrupted expression leaked (keep err on top)
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			vm_pop(vm);
			break;
		}

		if(sc->is_try) {
			sc->is_try = false; //consume: a throw inside this catch must not re-trigger the same handler (infinite loop)
			vm->pc = sc->pc;
			break;
		}
		vm_pop_scope(vm);
	}
}

/* Record an exception raised inside a native function. The actual unwinding is
 * done by func_call right after the native returns, so the value stack keeps
 * its env-pop / ret-push protocol balanced (throwing directly from a native
 * would leave the call env stranded on the stack). */
void vm_throw_native(vm_t* vm, const char *format, ...) {
	char message[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, BUF_SIZE, format, ap);
	va_end(ap);

	var_t* err = var_new_obj(vm, vm->builtin_vars.var_Error, NULL, NULL);
	var_t* msg = var_find_member_var(err, "message");
	if(msg != NULL)
		var_set_str(msg, message);
	if(vm->native_thrown != NULL)
		var_unref(vm->native_thrown);
	vm->native_thrown = var_ref(err);
}

/* Build a typed error instance ("TypeError"/"RangeError"/...): the class is
 * looked up by name and the instance's [[Prototype]] is set to that class's
 * prototype, so `e instanceof TypeError` and `e.name` behave correctly (unlike
 * vm_throw, which attaches the bare Error class var). Own `message` and `name`
 * members are created the same way native_Error's constructor does. Returned
 * with refs=0; the caller either pushes it (vm_throw_type) or defers it
 * (vm_throw_type_native). */
static var_t* vm_make_type_error(vm_t* vm, const char* type_name, const char* message) {
	node_t* cn = vm_load_node(vm, type_name, false);
	var_t* cls = (cn != NULL) ? cn->var : NULL;
	var_t* proto = (cls != NULL) ? var_get_prototype(cls) : NULL;
	if(proto == NULL)
		proto = vm->builtin_vars.var_Error;

	var_t* err = var_new_obj(vm, proto, NULL, NULL);
	node_t* nm = var_find_member(err, "message");
	if(nm != NULL && nm->var != NULL)
		var_set_str(nm->var, message);
	else
		var_add(err, "message", var_new_str(vm, message));
	node_t* nn = var_find_member(err, "name");
	if(nn != NULL && nn->var != NULL)
		var_set_str(nn->var, type_name);
	else
		var_add(err, "name", var_new_str(vm, type_name));
	return err;
}

/* Deferred typed throw for use INSIDE native functions: records the same typed
 * error instance vm_throw_type would raise in vm->native_thrown instead of
 * unwinding, so func_call delivers it to the nearest try scope after the native
 * returns and the value stack (env-pop / ret-push) stays balanced. The native
 * must still return a dummy value. */
void vm_throw_type_native(vm_t* vm, const char* type_name, const char* format, ...) {
	char message[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, BUF_SIZE, format, ap);
	va_end(ap);

	var_t* err = vm_make_type_error(vm, type_name, message);
	if(vm->native_thrown != NULL)
		var_unref(vm->native_thrown);
	vm->native_thrown = var_ref(err);
}

/* Throw a specific standard error type, unwinding the stack to the nearest
 * try/catch exactly like vm_throw(). For use from VM instruction handlers, NOT
 * from inside a native (see vm_throw_type_native for that). */
void vm_throw_type(vm_t* vm, const char* type_name, const char* format, ...) {
	char message[BUF_SIZE+1] = {0};
	va_list ap;
	va_start(ap, format);
	vsnprintf(message, BUF_SIZE, format, ap);
	va_end(ap);

	var_t* err = vm_make_type_error(vm, type_name, message);
	vm_push(vm, err);

	scope_t* try_sc = vm_get_try_catch_scope(vm);
	if(try_sc == NULL) {
		vm_pop(vm);
		return;
	}
	vm_throw_truncate(vm); // drop operands the interrupted expression leaked (keep err on top)
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			vm_pop(vm);
			break;
		}
		if(sc->is_try) {
			vm->pc = sc->pc;
			break;
		}
		vm_pop_scope(vm);
	}
}


static inline var_t* vm_this_in_scopes(vm_t* vm) {
	node_t* n = vm_find_in_scopes(vm, THIS);
	if(n == NULL)
		return NULL;
	return n->var;
}

/* The `super` binding of the enclosing method (func_call stores it in the call
 * env as the home object's [[Prototype]]). Used to recognise a `super.method()`
 * receiver so the call can look the method up on the prototype while keeping
 * `this` bound to the current instance. */
static inline var_t* vm_super_in_scopes(vm_t* vm) {
	node_t* n = vm_find_in_scopes(vm, SUPER);
	if(n == NULL)
		return NULL;
	return n->var;
}

inline node_t* vm_load_node(vm_t* vm, const char* name, bool create) {
	var_t* var = vm_get_scope_var(vm);

	node_t* n = NULL;
	if(var != NULL)
		n = var_find_member(var, name);
	if(n == NULL)
		n =  vm_find_in_scopes(vm, name);	

	if(n != NULL && n->var != NULL && n->var->status != V_ST_FREE)
		return n;

	if(!create) {
		mario_debug("Error: '%s' undefined!\n", name);	
		return NULL;
	}

	if(var == NULL)
		return NULL;

	n =var_add(var, name, NULL);	
	return n;
}

/*static void var_clone_members(var_t* var, var_t* src) {
	//clone member varibles.
	uint32_t i;
	for(i=0; i<src->children.size; i++) {
		node_t* node = (node_t*)src->children.items[i];
		if(node != NULL && !node->var->is_func && node->name[0] != 0) { //don't clone functions.
			if(strcmp(node->name, THIS) == 0 ||
					strcmp(node->name, PROTOTYPE) == 0 ||
					strcmp(node->name, CONSTRUCTOR) == 0) 
				continue;
			var_add(var, node->name, node->var);	
		}
	}
}
*/

void var_instance_from(var_t* var, var_t* src) {
	if(var == NULL || src == NULL)
		return;

	var_t* proto = var_get_prototype(src);
	if(proto == NULL)
		proto = src;
	var_set_prototype(var, proto);
}

static void var_set_father(var_t* var, var_t* father) {
	if(var == NULL)
		return;

	var_t* proto = var_get_prototype(var);
	if(proto == NULL)
		return;

	var_t* super_proto = NULL;
	if(father != NULL) 
		super_proto = var_get_prototype(father);
	if(super_proto == NULL)
		return;

	var_set_prototype(proto, super_proto);
}

//for function.
static func_t* func_new() {
	func_t* func = (func_t*)mario_malloc(sizeof(func_t));
	if(func == NULL)
		return NULL;
	memset(func, 0, sizeof(func_t));
	
	func->native = NULL;
	func->regular = true;
	func->pc = 0;
	func->data = NULL;
	func->owner = NULL;
	array_init(&func->args);
	return func;
}

static void func_free(void* p) {
	func_t* func = (func_t*)p;
	/* Defensive: var_free()'s is_func block normally detaches and releases this
	 * before func_free() runs (via var_clean). If a func_t is ever freed through
	 * another path, drop the pinned owner-var ref here so it is never leaked.
	 * Detach first so a recursive teardown can not re-enter this func_t. */
	if(func->closure_func_ref != NULL) {
		var_t* ref = func->closure_func_ref;
		func->closure_func_ref = NULL;
		var_unref(ref);
	}
	array_clean(&func->args, NULL);
	mario_free(p);
}

static var_t* var_new_func(vm_t* vm, func_t* func) {
	var_t* var = var_new_obj_no_proto(vm, NULL, NULL);
	var->is_func = 1;
	var->free_func = func_free;
	var->value = func;
	if(func != NULL)
		func->owner_var = var; //gc anchor: root this func_t via its owning var (see gc_mark is_func walk)
	var_t* proto = var_get_prototype(vm->builtin_vars.var_Object);
	if(proto == NULL)
		proto = var_new_obj_no_proto(vm, NULL, NULL);
	var_set_prototype(var, proto);
	//var_add(proto, CONSTRUCTOR, var);
	return var;
}

static var_t* find_func(vm_t* vm, var_t* obj, const char* fname) {
	//try full name with arg_num
	node_t* node = NULL;
	if(obj != NULL) {
		node = var_find_member(obj, fname);
	}
	if(node == NULL) {
		node = vm_find_in_scopes(vm, fname);
	}

	if(node != NULL && node->var != NULL && node->var->type == V_OBJECT) {
		return node->var;
	}
	return NULL;
}

static var_t* gen_create(vm_t* vm, var_t* func_var, var_t* env);

static bool func_call(vm_t* vm, var_t* obj, var_t* func_var, int arg_num) {
	/* Proxy apply/construct: a callable proxy has no func_t, so route it before the
	 * ordinary path builds an env. Collect the arg_num args the caller already pushed
	 * (stack order -> natural), then drive the apply trap - or the construct trap when
	 * vm->new_target marks a `new p(...)`. The trap functions themselves are ordinary
	 * is_func values, so this never recurses. */
	if(var_is_proxy(func_var)) {
		vm->gc.gc_defer++;
		var_t* pargs = var_new_array(vm);
		for(int i = 0; i < arg_num; i++) {
			var_t* a = vm_pop2(vm);
			var_array_add(pargs, (a != NULL) ? a : var_new(vm));
			if(a != NULL) var_unref(a);
		}
		var_array_reverse(pargs);
		var_t* res;
		if(vm->new_target != NULL) {
			var_t* nt = vm->new_target;
			vm->new_target = NULL;
			res = proxy_construct(vm, func_var, pargs, nt);
		}
		else {
			res = proxy_apply(vm, func_var, obj, pargs);
		}
		var_unref(pargs);
		vm_push(vm, (res != NULL) ? res : var_new(vm));
		vm->gc.gc_defer--;
		return true;
	}
	var_t *env = var_new_obj_no_proto(vm, NULL, NULL);
	var_t* args = var_new_array(vm);
	var_add(env, "arguments", args);
	if(func_var == NULL) {
		mario_debug("func_call: func_var is NULL!");
		var_unref(env);
		vm_push(vm, var_new(vm));
		return false;
	}
	func_t* func = var_get_func(func_var);
	if(func == NULL) {
		mario_debug("func_call: func is NULL!");
		var_unref(env);
		vm_push(vm, var_new(vm));
		return false;
	}
	if(obj == NULL) {
		//obj = vm->root;
	}
	else {
		var_add(env, THIS, obj);
	}

	/* env/args are freshly built and not yet rooted on the stack (that happens
	 * with vm_push(env) below). A gc triggered while collecting arguments would
	 * see the just-added argument values as unreachable and free them, leaving
	 * dangling pointers in args (UAF at var_array_reverse). Defer it until env
	 * is pushed. */
	vm->gc.gc_defer++;

	int32_t i;
	for(i=arg_num; i>func->args.size; i--) {
		var_t* v = vm_pop2(vm);
		var_array_add(args, v);
		var_unref(v);
	}

	for(i=(int32_t)func->args.size-1; i>=0; i--) {
		const char* arg_name = (const char*)array_get(&func->args, i);
		var_t* v = NULL;
		if(i >= arg_num) {
			v = var_new(vm);
			var_ref(v);
		}
		else {
			v = vm_pop2(vm);	
		}	
		if(v != NULL) {
			var_array_add(args, v);
			var_add(env, arg_name, v);
			var_unref(v);
		}
	}
	var_array_reverse(args); // reverse the args array coz stack index.

	if(func->owner != NULL) {
		var_t* super_v = var_get_prototype(func->owner);
		if(super_v != NULL)
			var_add(env, SUPER, super_v);
	}

	/* ES6 `new.target`: new_obj() sets vm->new_target to the constructor right
	 * before invoking it. Bind it into this call's env and consume it, so the
	 * constructor body reads `new.target` while any nested plain call (which
	 * finds vm->new_target already cleared) reads undefined. */
	if(vm->new_target != NULL) {
		var_add(env, "@new.target", vm->new_target);
		vm->new_target = NULL;
	}

	var_t* ret = NULL;
	vm_push(vm, env); //avoid for gc
	vm->gc.gc_defer--; //env is now rooted on the stack; gc is safe again.
	if(func->native != NULL) { //native function
		ret = func->native(vm, env, func->data);
		if(ret == NULL)
			ret = var_new(vm);
	}
	else if(func->is_generator) {
		/* ES6 `function*`: calling a generator function does not run the body;
		 * it builds a suspended generator object driven by next()/return()/throw(). */
		ret = gen_create(vm, func_var, env);
	}
	else {
		scope_t* sc = scope_new(env);
		sc->pc = vm->pc;
		sc->is_func = true;
		sc->func = func;
		sc->func_var = func_var; //root the function object so its func_t survives gc during the body
		sc->stack_top = vm->stack_top; //frame baseline (env already pushed): a throw truncates leaked operands down to here

		vm_push_scope(vm, sc);

		//script function
		vm->pc = func->pc;
		if(vm_run(vm)) { //with function return;
			ret = vm_pop2(vm);
			ret->refs--;
		}
		//scope's already poped with function return
	}

	if(ret == NULL)
		ret = var_new(vm);
	var_ref(ret);
	/* Between popping env and pushing ret the return value's object graph is
	 * unreachable; an opportunistic gc here would sweep its children. */
	vm->gc.gc_defer++;
	vm_pop(vm);
	if(vm->native_thrown != NULL) { //a native raised: deliver the error instead of ret
		var_t* err = vm->native_thrown;
		vm->native_thrown = NULL;
		var_unref(ret); //drop the would-be return value
		/* The receiver (obj) of this call is now held only by the caller's C frame
		 * (env is already popped); a gc during the unwind would sweep it and the
		 * caller's var_unref(obj) becomes a UAF. */
		vm->gc.gc_defer++;
		vm_push(vm, err);
		var_unref(err); //the stack ref keeps it alive now
		vm_throw_truncate(vm); //drop operands the caller's interrupted expression leaked (e.g. `let x = nativeCall()`)
		while(true) { //same unwinding as handle_throw
			scope_t* sc = vm_get_scope(vm);
			if(sc == NULL) {
				mario_printf("Error: uncaught exception from native function!\n");
				vm_terminate(vm);
				break;
			}
			if(sc->is_try) {
				sc->is_try = false; //consume: a throw inside this catch must not re-trigger the same handler (infinite loop)
				vm->pc = sc->pc;
				break;
			}
			vm_pop_scope(vm);
		}
		vm->gc.gc_defer--;
		vm->gc.gc_defer--; //the outer ret-delivery guard
		return true;
	}
	ret->refs--;
	vm_push(vm, ret);
	vm->gc.gc_defer--;
	return true;
}

static var_t* func_def(vm_t* vm, int regular, bool is_static) {
	func_t* func = func_new();
	func->regular = regular;
	func->is_static = is_static;
	while(true) {
		PC ins = vm->bc.code_buf[vm->pc++];
		opr_code_t instr = OP(ins);
		uint32_t offset = OFF(ins);
		if(instr == INSTR_JMP) {
			func->pc = vm->pc;
			vm->pc = vm->pc + offset - 1;
			break;
		}

		const char* s = bc_getstr(&vm->bc, offset);
		if(s == NULL)
			break;
		array_add_buf(&func->args, (void*)s, (uint32_t)strlen(s) + 1);
	}

	var_t* ret = var_new_func(vm, func);
	return ret;
}

static inline bool is_compound_assign(opr_code_t op) {
	switch(op) {
		case INSTR_PLUSEQ:
		case INSTR_MINUSEQ:
		case INSTR_MULTIEQ:
		case INSTR_DIVEQ:
		case INSTR_MODEQ:
		case INSTR_POWEQ:
		case INSTR_BITANDEQ:
		case INSTR_BITOREQ:
		case INSTR_BITXOREQ:
		case INSTR_LSHIFTEQ:
		case INSTR_RSHIFTEQ:
		case INSTR_URSHIFTEQ:
			return true;
		default:
			return false;
	}
}

/* If `n` is a synthetic @@taslot write-target (INSTR_ARRAY_AT_W), encode `val`
 * into the referenced TypedArray element and return true (the caller frees the
 * sentinel with node_free instead of node_replace). False for a normal binding
 * node, so array/object/variable targets are untouched. The @@taslot check is a
 * single byte test plus strcmp on the sentinel name only. */
static bool ta_slot_write(vm_t* vm, node_t* n, var_t* val) {
	if(n == NULL || n->name == NULL || n->name[0] != '@' || strcmp(n->name, TA_SLOT) != 0)
		return false;
	var_t* ta = var_find_own_member_var(n->var, TA_SLOT_TA);
	int64_t idx = (int64_t)(int32_t)n->ncache_instr;
	if(ta != NULL)
		var_typedarray_set_at(vm, ta, idx, val);
	return true;
}

/* ---- Proxy write sentinel (Phase 5) ----
 * `proxy.name = v` / `proxy[k] = v` compile to the same GETW / ARRAY_AT_W target
 * fetch as a plain assignment, so the write path routes a proxy through a synthetic
 * @@proxyslot node exactly like a TypedArray's @@taslot. The node's ->var is a lazy
 * undefined placeholder carrying hidden @@pobj (the proxy) and @@pkey (the key);
 * proxy_slot_write() detects the sentinel and drives the set trap.
 *
 * The current value is resolved LAZILY: a simple `p.x = v` never reads it (so only
 * the set trap fires, per spec), while a compound `p.x += v` / `p.x++` resolves it
 * through the get trap in handle_math / vm_step_op before computing. Both of those
 * discard the placeholder and substitute the get-trap result as the left operand. */
static inline bool is_proxy_slot(node_t* n) {
	return n != NULL && n->name != NULL && n->name[0] == '@' && strcmp(n->name, PROXY_SLOT) == 0;
}

static inline var_t* proxy_slot_obj(node_t* n) { return var_find_own_member_var(n->var, PROXY_SLOT_OBJ); }
static inline var_t* proxy_slot_key(node_t* n) { return var_find_own_member_var(n->var, PROXY_SLOT_KEY); }

/* Push a @@proxyslot write-target for `p[key] = ..`. Adopts one reference each on
 * `p` and `key` (via the hidden members); the caller releases its own references
 * (the popped stack refs), leaving the sentinel as the sole holder until it is
 * consumed by proxy_slot_write / node_free. */
static void proxy_push_slot(vm_t* vm, var_t* p, var_t* key) {
	var_t* cur = var_new(vm);   /* lazy placeholder; see is_proxy_slot comment */
	vm->gc.gc_defer++;          /* sn/cur are unrooted until vm_push_node below */
	var_ref(cur);               /* node's own reference */
	node_t* sn = (node_t*)mario_malloc(sizeof(node_t));
	memset(sn, 0, sizeof(node_t));
	sn->magic = 1;
	sn->name = (char*)mario_malloc(strlen(PROXY_SLOT)+1);
	memcpy(sn->name, PROXY_SLOT, strlen(PROXY_SLOT)+1);
	sn->var = cur;
	node_t* on = var_add(cur, PROXY_SLOT_OBJ, p);      /* var_add refs p */
	on->invisable = 1; on->be_unenumerable = 1;
	node_t* kn = var_add(cur, PROXY_SLOT_KEY, key);    /* var_add refs key */
	kn->invisable = 1; kn->be_unenumerable = 1;
	vm_push_node(vm, sn);                              /* adds the stack reference to cur */
	vm->gc.gc_defer--;
}

/* If `n` is a @@proxyslot write-target, drive the proxy set trap with `val` and
 * return true (the caller frees the sentinel with node_free instead of
 * node_replace). The hidden @@pobj/@@pkey back-refs are removed afterwards so a
 * shared current value is never left polluted (or leaking a proxy reference). */
static bool proxy_slot_write(vm_t* vm, node_t* n, var_t* val) {
	if(!is_proxy_slot(n))
		return false;
	var_t* p = proxy_slot_obj(n);
	var_t* k = proxy_slot_key(n);
	if(p != NULL && k != NULL) {
		if(val != NULL) var_ref(val);   /* protect val across the trap call */
		proxy_set(vm, p, k, val, p);
		if(val != NULL) var_unref(val);
	}
	var_delete_own_member(n->var, PROXY_SLOT_OBJ);
	var_delete_own_member(n->var, PROXY_SLOT_KEY);
	return true;
}


/* Publish an arithmetic result. For a compound assignment (`x += y`) the freshly
 * built result var is installed through the lvalue's node instead of being
 * written into v1 in place: one var_t is shared by every alias of a binding
 * (`var b = a;` leaves both nodes referencing the same var) and integer literals
 * are shared through vm->var_cache, so an in-place write corrupts all of them. */
static inline void math_result(vm_t* vm, opr_code_t op, node_t* n, var_t* res) {
	if(n != NULL && is_compound_assign(op)) {
		/* A synthetic @@taslot target (INSTR_ARRAY_AT_W): encode the result into
		 * the TypedArray's buffer and free the sentinel instead of node_replace
		 * (which would only rebind the throw-away node->var). A @@proxyslot target
		 * drives the proxy set trap and is freed the same way. */
		if(ta_slot_write(vm, n, res) || proxy_slot_write(vm, n, res))
			node_free(n);
		else
			node_replace(n, res); //the node takes its own reference to res.
	}
	vm_push(vm, res);
}

/* Numeric width class for arithmetic promotion. int32/int64 are exact integer
 * lanes; float32 and double both compute as double (the canonical float). */
#define NC_NONE  0
#define NC_INT32 1
#define NC_INT64 2
#define NC_FLOAT 3

static inline int num_class(var_t* v) {
	switch(v->type) {
		case V_INT:     return NC_INT32;
		case V_INT64:   return NC_INT64;
		case V_FLOAT:
		case V_FLOAT64: return NC_FLOAT;
		default:        return NC_NONE;
	}
}

/* Box an integer arithmetic result at the narrowest exact width. Once it leaves
 * the JS safe-integer range (|r| > 2^53-1) JS itself could only hold an
 * approximate double, so spill to V_FLOAT64 to match. */
static inline var_t* box_int_result(vm_t* vm, int64_t r) {
	const int64_t SAFE = 9007199254740991LL; // 2^53 - 1
	if(r > SAFE || r < -SAFE)
		return var_new_float64(vm, (double)r);
	if(r >= -2147483648LL && r <= 2147483647LL)
		return var_new_int(vm, (int)r);
	return var_new_int64(vm, r);
}

/* JS ToInt32 for the bitwise operators: exact for the integer lanes, and for a
 * double it truncates toward zero, wraps modulo 2^32 and reinterprets as signed
 * (NaN/Infinity -> 0). */
static inline int32_t to_int32(var_t* v) {
	double d;
	switch(v->type) {
		case V_INT:     return *(int*)v->value;
		case V_INT64:   return (int32_t)(*(int64_t*)v->value);
		case V_FLOAT:   d = (double)(*(float*)v->value); break;
		case V_FLOAT64: d = *(double*)v->value; break;
		default:        return 0;
	}
	if(isnan(d) || isinf(d))
		return 0;
	double m = fmod(trunc(d), 4294967296.0);
	int64_t i = (int64_t)m;
	if(i < 0)
		i += 4294967296LL;
	return (int32_t)(uint32_t)i;
}

/* Compute a (+|-|*) b in int64; return true on signed overflow (the caller then
 * falls back to double, which is what JS does beyond the exact int64 range). */
static inline bool int64_arith(opr_code_t op, int64_t a, int64_t b, int64_t* out) {
	if(op == INSTR_PLUS || op == INSTR_PLUSEQ) {
		if((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return true;
		*out = a + b; return false;
	}
	if(op == INSTR_MINUS || op == INSTR_MINUSEQ) {
		if((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return true;
		*out = a - b; return false;
	}
	// multiply
	if(a == 0 || b == 0) { *out = 0; return false; }
	if(a > 0) {
		if(b > 0) { if(a > INT64_MAX / b) return true; }
		else      { if(b < INT64_MIN / a) return true; }
	} else {
		if(b > 0) { if(a < INT64_MIN / b) return true; }
		else      { if(b < INT64_MAX / a) return true; }
	}
	*out = a * b; return false;
}

static inline void math_op(vm_t* vm, opr_code_t op, var_t* v1, var_t* v2, node_t* n) {
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}

	/* ---- BigInt lane ----
	 * BigInt arithmetic is exact and never silently mixes with Number. When both
	 * operands are BigInt, `+ - * / % ** & | ^ << >>` stay BigInt (bitwise is
	 * arbitrary-width two's complement; `>>` is an arithmetic/floor shift). Any
	 * mix of BigInt with another type is a TypeError except `+` with a string or
	 * object, which concatenates. `>>>` is meaningless for BigInt (TypeError).
	 * Division/modulo by zero, a negative exponent, or a negative shift count are
	 * RangeError. (Bitwise compound assigns like `&=` have their own opcodes and
	 * ride the same lanes; math_result() writes back through the lvalue node.) */
	if(v1->type == V_BIGINT || v2->type == V_BIGINT) {
		if(op == INSTR_URSHIFT || op == INSTR_URSHIFTEQ) {
			vm_throw_type(vm, "TypeError", "BigInts have no unsigned right shift");
			return;
		}
		if(v1->type == V_BIGINT && v2->type == V_BIGINT) {
			bignum_t* x = (bignum_t*)v1->value;
			bignum_t* y = (bignum_t*)v2->value;
			bignum_t* r = NULL;
			switch(op) {
				case INSTR_PLUS:   case INSTR_PLUSEQ:   r = bn_add(x, y); break;
				case INSTR_MINUS:  case INSTR_MINUSEQ:  r = bn_sub(x, y); break;
				case INSTR_MULTI:  case INSTR_MULTIEQ:  r = bn_mul(x, y); break;
				case INSTR_DIV:    case INSTR_DIVEQ:
					if(bn_is_zero(y)) { vm_throw_type(vm, "RangeError", "Division by zero"); return; }
					r = bn_div(x, y); break;
				case INSTR_MOD:    case INSTR_MODEQ:
					if(bn_is_zero(y)) { vm_throw_type(vm, "RangeError", "Division by zero"); return; }
					r = bn_mod(x, y); break;
				case INSTR_POW:    case INSTR_POWEQ:
					if(y->sign < 0) { vm_throw_type(vm, "RangeError", "BigInt negative exponent"); return; }
					r = bn_pow(x, y); break;
				case INSTR_AND:    case INSTR_BITANDEQ:  r = bn_and(x, y); break;
				case INSTR_OR:     case INSTR_BITOREQ:   r = bn_or(x, y); break;
				case INSTR_XOR:    case INSTR_BITXOREQ:  r = bn_xor(x, y); break;
				case INSTR_LSHIFT: case INSTR_LSHIFTEQ:
					if(y->sign < 0) { vm_throw_type(vm, "RangeError", "Negative shift count"); return; }
					r = bn_shl(x, (uint32_t)bn_to_int64(y)); break;
				case INSTR_RSHIFT: case INSTR_RSHIFTEQ:
					if(y->sign < 0) { vm_throw_type(vm, "RangeError", "Negative shift count"); return; }
					r = bn_shr(x, (uint32_t)bn_to_int64(y)); break;
				default:
					vm_throw_type(vm, "TypeError", "Cannot mix BigInt and other types");
					return;
			}
			math_result(vm, op, n, var_new_bigint(vm, r ? r : bn_new()));
			return;
		}
		/* Mixed BigInt / non-BigInt: `+` concatenates with a string or object,
		 * everything else is a TypeError. */
		if(op == INSTR_PLUS || op == INSTR_PLUSEQ) {
			var_t* other = (v1->type == V_BIGINT) ? v2 : v1;
			if(other->type == V_STRING || other->type == V_OBJECT) {
				mstr_t* s = mstr_new("");
				var_to_str(v1, s);
				mstr_t* s2 = mstr_new("");
				var_to_str(v2, s2);
				mstr_append(s, s2->cstr);
				mstr_free(s2);
				var_t* v = var_new_str(vm, s->cstr);
				mstr_free(s);
				math_result(vm, op, n, v);
				return;
			}
		}
		vm_throw_type(vm, "TypeError", "Cannot mix BigInt and other types");
		return;
	}

	int c1 = num_class(v1);
	int c2 = num_class(v2);

	//ES6 exponent operator ** : always double pow, box integral results exactly.
	if(op == INSTR_POW || op == INSTR_POWEQ) {
		double b = (c1 == NC_NONE) ? 0.0 : var_get_float64(v1);
		double e = (c2 == NC_NONE) ? 0.0 : var_get_float64(v2);
		double r = pow(b, e);
		/* Whole-number results inside the int64 range are boxed as integers
		 * (5 ** 0 -> int 1, 2 ** 50 -> int64); box_int_result() still spills
		 * past 2^53 so 2 ** 53 matches the JS double. */
		if(!isnan(r) && !isinf(r) && r == floor(r) && fabs(r) < 9223372036854775808.0)
			math_result(vm, op, n, box_int_result(vm, (int64_t)r));
		else
			math_result(vm, op, n, var_new_float64(vm, r));
		return;
	}

	//Bitwise ops use JS ToInt32/ToUint32 semantics: the result is a 32-bit
	//integer regardless of operand width, so they never promote to int64/double.
	//The `op=` compound forms share the same computation; math_result() writes the
	//result back through the lvalue node because is_compound_assign() is true.
	if((op == INSTR_AND || op == INSTR_OR || op == INSTR_XOR ||
	    op == INSTR_LSHIFT || op == INSTR_RSHIFT || op == INSTR_URSHIFT ||
	    op == INSTR_BITANDEQ || op == INSTR_BITOREQ || op == INSTR_BITXOREQ ||
	    op == INSTR_LSHIFTEQ || op == INSTR_RSHIFTEQ || op == INSTR_URSHIFTEQ) &&
	   c1 != NC_NONE && c2 != NC_NONE) {
		int32_t a = to_int32(v1);
		int32_t b = to_int32(v2);
		int32_t sh = b & 31;
		switch(op) {
			case INSTR_AND:     case INSTR_BITANDEQ: math_result(vm, op, n, var_new_int(vm, a & b)); return;
			case INSTR_OR:      case INSTR_BITOREQ:  math_result(vm, op, n, var_new_int(vm, a | b)); return;
			case INSTR_XOR:     case INSTR_BITXOREQ: math_result(vm, op, n, var_new_int(vm, a ^ b)); return;
			case INSTR_LSHIFT:  case INSTR_LSHIFTEQ: math_result(vm, op, n, var_new_int(vm, a << sh)); return;
			case INSTR_RSHIFT:  case INSTR_RSHIFTEQ: math_result(vm, op, n, var_new_int(vm, a >> sh)); return;
			case INSTR_URSHIFT: case INSTR_URSHIFTEQ: {
				uint32_t ur = ((uint32_t)a) >> sh; // unsigned: 0..2^32-1
				math_result(vm, op, n, box_int_result(vm, (int64_t)ur));
				return;
			}
		}
	}

	// Arithmetic + - * / % when both operands are numeric.
	if(c1 != NC_NONE && c2 != NC_NONE) {
		// Division always yields a double in JS (IEEE handles /0 -> Inf/NaN).
		if(op == INSTR_DIV || op == INSTR_DIVEQ) {
			math_result(vm, op, n, var_new_float64(vm, var_get_float64(v1) / var_get_float64(v2)));
			return;
		}
		if(c1 == NC_FLOAT || c2 == NC_FLOAT) {
			// Any float operand -> double arithmetic (canonical V_FLOAT64).
			double d1 = var_get_float64(v1), d2 = var_get_float64(v2), r = 0.0;
			switch(op) {
				case INSTR_PLUS:   case INSTR_PLUSEQ:   r = d1 + d2; break;
				case INSTR_MINUS:  case INSTR_MINUSEQ:  r = d1 - d2; break;
				case INSTR_MULTI:  case INSTR_MULTIEQ:  r = d1 * d2; break;
				case INSTR_MOD:    case INSTR_MODEQ:    r = fmod(d1, d2); break;
			}
			math_result(vm, op, n, var_new_float64(vm, r));
			return;
		}
		// Both integral (int32/int64): compute in int64, spill to double on
		// overflow; modulo stays integral (JS `%` truncates).
		int64_t a = var_get_int64(v1), b = var_get_int64(v2);
		if(op == INSTR_MOD || op == INSTR_MODEQ) {
			if(b == 0) { math_result(vm, op, n, var_new_float64(vm, NAN)); return; }
			int64_t r = (a == INT64_MIN && b == -1) ? 0 : (a % b);
			math_result(vm, op, n, box_int_result(vm, r));
			return;
		}
		int64_t r;
		if(int64_arith(op, a, b, &r)) {
			double da = (double)a, db = (double)b, dr;
			if(op == INSTR_PLUS || op == INSTR_PLUSEQ) dr = da + db;
			else if(op == INSTR_MINUS || op == INSTR_MINUSEQ) dr = da - db;
			else dr = da * db;
			math_result(vm, op, n, var_new_float64(vm, dr));
		} else {
			math_result(vm, op, n, box_int_result(vm, r));
		}
		return;
	}

	//do string + 
	if(op == INSTR_PLUS || op == INSTR_PLUSEQ) {
		/* Convert the left operand to a string properly. Casting v1->value to
		 * char* directly produced garbage when v1 was a non-string, e.g.
		 * `5 + "x"` read the int's raw bytes as a C string. */
		mstr_t* s = mstr_new("");
		var_to_str(v1, s);
		mstr_t* json = mstr_new("");
		var_to_str(v2, json);
		mstr_append(s, json->cstr);
		mstr_free(json);

		var_t* v = var_new_str(vm, s->cstr);
		mstr_free(s);
		math_result(vm, op, n, v);
		return;
	}

	/* Neither numeric nor a concatenation (e.g. `"a" - 1`, where JS yields
	 * NaN). Push undefined so the value stack stays balanced. */
	vm_push(vm, var_new(vm));
}

static inline void compare(vm_t* vm, opr_code_t op, var_t* v1, var_t* v2) {
    if(v1->type == V_OBJECT) {
        bool i = false;
        switch(op) {
        case INSTR_EQ:
        case INSTR_TEQ:
            i = (v1 == v2);
            break;
        case INSTR_NEQ:
        case INSTR_NTEQ:
            i = (v1 != v2);
            break;
        }
        if(i)
            vm_push(vm, vm->builtin_vars.var_true);
        else
            vm_push(vm, vm->builtin_vars.var_false);
        return;
    }
    
	/* ---- BigInt comparisons ----
	 * bigint vs bigint: exact, and `===`/`==` agree (same type). bigint vs a
	 * Number: `===`/`!==` are false/true (different types) while `==`/`!=` and the
	 * relational ops compare by mathematical value — exactly against an integer
	 * operand, via double for a fractional/huge one, and NaN makes every
	 * comparison false (except !=/!==). bigint vs bool/string/null/undefined/
	 * object falls through to the generic loose-equality rules below, matching how
	 * this VM already treats other mixed-type comparisons. */
	if(v1->type == V_BIGINT || v2->type == V_BIGINT) {
		bool both = (v1->type == V_BIGINT && v2->type == V_BIGINT);
		var_t* ov = (v1->type == V_BIGINT) ? v2 : v1;
		bool numeric = (ov->type == V_INT || ov->type == V_INT64 ||
		                ov->type == V_FLOAT || ov->type == V_FLOAT64);
		if(both || numeric) {
			int c;
			if(both) {
				c = bn_cmp((bignum_t*)v1->value, (bignum_t*)v2->value);
			}
			else {
				bignum_t* b = (bignum_t*)((v1->type == V_BIGINT) ? v1->value : v2->value);
				int local;
				if(ov->type == V_INT || ov->type == V_INT64) {
					bignum_t* ob = bn_from_int64(var_get_int64(ov));
					local = bn_cmp(b, ob);
					bn_free(ob);
				}
				else {
					double od = var_get_float64(ov);
					if(isnan(od)) {
						bool ni = (op == INSTR_NEQ || op == INSTR_NTEQ);
						vm_push(vm, ni ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
						return;
					}
					local = bn_cmp_double(b, od);
				}
				c = (v1->type == V_BIGINT) ? local : -local;
			}
			bool i = false;
			switch(op) {
				case INSTR_TEQ: i = both ? (c == 0) : false; break;
				case INSTR_NTEQ: i = both ? (c != 0) : true; break;
				case INSTR_EQ:  i = (c == 0); break;
				case INSTR_NEQ: i = (c != 0); break;
				case INSTR_LES: i = (c < 0); break;
				case INSTR_GRT: i = (c > 0); break;
				case INSTR_LEQ: i = (c <= 0); break;
				case INSTR_GEQ: i = (c >= 0); break;
			}
			vm_push(vm, i ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
			return;
		}
	}

	// Both integers (int32/int64): compare exactly in int64 (no double rounding).
	if((v1->type == V_INT || v1->type == V_INT64) &&
	   (v2->type == V_INT || v2->type == V_INT64)) {
		int64_t i1 = var_get_int64(v1);
		int64_t i2 = var_get_int64(v2);
		bool i = false;
		switch(op) {
			case INSTR_EQ:
			case INSTR_TEQ:  i = (i1 == i2); break;
			case INSTR_NEQ:
			case INSTR_NTEQ: i = (i1 != i2); break;
			case INSTR_LES:  i = (i1 < i2); break;
			case INSTR_GRT:  i = (i1 > i2); break;
			case INSTR_LEQ:  i = (i1 <= i2); break;
			case INSTR_GEQ:  i = (i1 >= i2); break;
		}
		vm_push(vm, i ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
		return;
	}

	// Any float involved (or mixed numeric): coerce both to double and compare.
	if(var_is_number(v1) && var_is_number(v2)) {
		double f1 = var_get_float64(v1);
		double f2 = var_get_float64(v2);
		bool i = false;
		switch(op) {
			case INSTR_EQ:
			case INSTR_TEQ:  i = (f1 == f2); break;
			case INSTR_NEQ:
			case INSTR_NTEQ: i = (f1 != f2); break;
			case INSTR_LES:  i = (f1 < f2); break;
			case INSTR_GRT:  i = (f1 > f2); break;
			case INSTR_LEQ:  i = (f1 <= f2); break;
			case INSTR_GEQ:  i = (f1 >= f2); break;
		}
		vm_push(vm, i ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
		return;
	}

	bool i = false;
	if(v1->type == v2->type) {
		if(v1->type == V_STRING) {
			switch(op) {
				case INSTR_EQ: 
				case INSTR_TEQ:
					i = (strcmp((const char*)v1->value, (const char*)v2->value) == 0);
					break; 
				case INSTR_NEQ: 
				case INSTR_NTEQ:
					i = (strcmp((const char*)v1->value, (const char*)v2->value) != 0);
					break;
			}
		}
		else if(v1->type == V_NULL) {
			switch(op) {
				case INSTR_EQ: 
				case INSTR_TEQ:
					i = (v2->type == V_NULL);
					break; 
				case INSTR_NEQ: 
				case INSTR_NTEQ:
					i = (v2->type != V_NULL);
					break;
			}
		}
		else if(v1->type == V_BOOL) {
			/* V_BOOL was not handled here, so `true === true` fell through and
			 * always yielded false. Compare booleans by value. */
			bool b1 = var_get_bool(v1);
			bool b2 = var_get_bool(v2);
			switch(op) {
				case INSTR_EQ:
				case INSTR_TEQ:
					i = (b1 == b2);
					break;
				case INSTR_NEQ:
				case INSTR_NTEQ:
					i = (b1 != b2);
					break;
			}
		}
		else if(v1->type == V_UNDEF) {
			/* Same-type block: both operands are undefined, which compares equal
			 * under both == and === (previously fell through to false). */
			i = (op == INSTR_EQ || op == INSTR_TEQ);
		}
	}
	else if((v1->type == V_UNDEF && v2->type == V_NULL) ||
		(v1->type == V_NULL && v2->type == V_UNDEF)) {
		/* JS: undefined == null is true (loose); undefined === null is false. */
		i = (op == INSTR_EQ || op == INSTR_NTEQ);
	}
	else if(op == INSTR_NEQ || op == INSTR_NTEQ) {
		i = true;
	}

	if(i)
		vm_push(vm, vm->builtin_vars.var_true);
	else
		vm_push(vm, vm->builtin_vars.var_false);
}

/* ES6 accessors: a getter/setter is a function var whose func_t.regular is
 * FUNC_GETTER/FUNC_SETTER. When both are defined for one property the getter is
 * the primary member var and the setter hangs off it as the hidden member
 * FUNC_SETTER_KEY (see merge_accessor). */
static inline bool var_is_accessor(var_t* v) {
	if(v == NULL || !v->is_func)
		return false;
	func_t* f = var_get_func(v);
	return f != NULL && (f->regular == FUNC_GETTER || f->regular == FUNC_SETTER);
}

/* The getter of an accessor property (NULL when it is write-only). */
static inline var_t* var_accessor_getter(var_t* v) {
	if(v == NULL || !v->is_func)
		return NULL;
	func_t* f = var_get_func(v);
	if(f == NULL)
		return NULL;
	return (f->regular == FUNC_GETTER) ? v : NULL;
}

/* The setter of an accessor property (NULL when it is read-only). */
static inline var_t* var_accessor_setter(var_t* v) {
	if(v == NULL || !v->is_func)
		return NULL;
	func_t* f = var_get_func(v);
	if(f == NULL)
		return NULL;
	if(f->regular == FUNC_SETTER)
		return v;
	if(f->regular == FUNC_GETTER)
		return get_obj(v, FUNC_SETTER_KEY);
	return NULL;
}

/* Merge a freshly compiled accessor `acc` into the existing accessor node `ex`
 * so the property keeps a single primary var (the getter when both exist),
 * carrying the counterpart as a hidden member. */
static void merge_accessor(node_t* ex, var_t* acc) {
	var_t* ex_var = ex->var;
	var_t* getter = NULL;
	var_t* setter = NULL;
	func_t* ef = var_get_func(ex_var);
	if(ef != NULL) {
		if(ef->regular == FUNC_GETTER) getter = ex_var;
		else if(ef->regular == FUNC_SETTER) setter = ex_var;
	}
	func_t* af = var_get_func(acc);
	if(af != NULL) {
		if(af->regular == FUNC_GETTER) getter = acc;
		else if(af->regular == FUNC_SETTER) setter = acc;
	}

	var_t* primary = (getter != NULL) ? getter : setter;
	if(primary == NULL)
		return;
	if(getter != NULL && setter != NULL) {
		node_t* sn = var_add(getter, FUNC_SETTER_KEY, setter);
		sn->invisable = 1;
		sn->be_unenumerable = 1;
	}
	if(primary != ex_var)
		node_replace(ex, primary); //refs primary, releases the node's old var.
}

/* JS string .length counts UTF-16 code units. mario stores string values as
 * UTF-8, so decode each code point and count 2 for astral ones (>0xFFFF, i.e. a
 * surrogate pair in UTF-16) and 1 otherwise. Pure ASCII counts 1 per byte, so
 * this matches the old strlen() behaviour for the common case. */
static int mstr_utf16_length(const char* s) {
	if(s == NULL)
		return 0;
	const unsigned char* p = (const unsigned char*)s;
	int units = 0;
	while(*p != 0) {
		unsigned char c = *p;
		uint32_t cp;
		if(c < 0x80) { cp = c; p += 1; }
		else if((c >> 5) == 0x6 && p[1] != 0) {
			cp = (uint32_t)(c & 0x1F) << 6 | (uint32_t)(p[1] & 0x3F); p += 2;
		}
		else if((c >> 4) == 0xE && p[1] != 0 && p[2] != 0) {
			cp = (uint32_t)(c & 0x0F) << 12 | (uint32_t)(p[1] & 0x3F) << 6 |
					(uint32_t)(p[2] & 0x3F); p += 3;
		}
		else if((c >> 3) == 0x1E && p[1] != 0 && p[2] != 0 && p[3] != 0) {
			cp = (uint32_t)(c & 0x07) << 18 | (uint32_t)(p[1] & 0x3F) << 12 |
					(uint32_t)(p[2] & 0x3F) << 6 | (uint32_t)(p[3] & 0x3F); p += 4;
		}
		else { cp = c; p += 1; } /* stray/invalid byte: count as one unit */
		units += (cp > 0xFFFF) ? 2 : 1;
	}
	return units;
}

/* Member fetch. When `for_write` is set the access is an assignment target: an
 * accessor property pushes the object (for the setter's `this`) followed by the
 * node so handle_asign can invoke the setter; a read invokes the getter and
 * pushes its result. */
void do_get(vm_t* vm, var_t* v, const char* name, bool for_write) {
	/* Proxy intercept: `p.name` routes the get trap (read) or pushes a @@proxyslot
	 * write-target (assignment). One hash miss for every non-proxy, so the plain
	 * path below is untouched. */
	if(var_is_proxy(v)) {
		var_t* keyv = var_new_str(vm, name);
		if(for_write) {
			proxy_push_slot(vm, v, keyv);   // sentinel adopts keyv (refs 0 -> 1)
		}
		else {
			var_ref(keyv);                  // own it for proxy_get's borrow contract
			var_t* res = proxy_get(vm, v, keyv, v);
			vm_push(vm, (res != NULL) ? res : var_new(vm));
			var_unref(keyv);
		}
		return;
	}
	if(v->type == V_STRING && strcmp(name, "length") == 0) {
		int len = mstr_utf16_length(var_get_str(v));
		vm_push(vm, var_new_int(vm, len));
		return;
	}
	else if(v->is_array && strcmp(name, "length") == 0) {
		int len = var_array_size(v);
		vm_push(vm, var_new_int(vm, len));
		return;
	}	

	node_t* n = var_find_member(v, name);
	if(n != NULL && var_is_accessor(n->var)) {
		if(for_write) {
			vm_push(vm, v);        //object for the setter's `this`
			vm_push_node(vm, n);   //accessor node; handle_asign calls the setter
			return;
		}
		var_t* getter = var_accessor_getter(n->var);
		if(getter != NULL) {
			var_ref(getter);
			func_call(vm, v, getter, 0); //pushes the computed value
			var_unref(getter);
		}
		else {
			vm_push(vm, var_new(vm)); //write-only property reads as undefined
		}
		return;
	}

	if(n == NULL) {
		if(v->type == V_UNDEF)
			v->type = V_OBJECT;

		if(v->type == V_OBJECT) {
			n = var_add(v, name, NULL);
		}
		else {
			mario_debug("Can not get member '%s'!\n", name);
			n = node_new(vm, name, NULL);
			vm_push(vm, var_new(vm));
			return;
		}
	}

	/* If v is transient the caller releases it right after we return, which
	 * would free the node we push (e.g. `getObj().prop`). Push the value in
	 * that case; keep node semantics for persistent objects so member
	 * assignment (obj.x = v) can still write through the node. */
	if(v->refs <= 1)
		vm_push(vm, n->var);
	else
		vm_push_node(vm, n);
}
static void do_extends(vm_t* vm, var_t* cls_var, const char* super_name) {
	node_t* n = vm_find_in_scopes(vm, super_name);
	if(n == NULL) {
		mario_debug("Super Class '%s' not found!\n", super_name);
		return;
	}

	var_set_father(cls_var, n->var);
}

/** create object by classname or function */
var_t* new_obj(vm_t* vm, const char* name, int arg_num) {
	var_t* obj = NULL;
	node_t* n = vm_load_node(vm, name, false); //load class;

	if(n == NULL || n->var->type != V_OBJECT) {
		mario_debug("Error: There is no class: '%s'!\n", name);
		vm_throw(vm, "there is no class: '%s'!", name);
		return NULL;
	}

	/* Proxy construct: `new p(...)` on a callable proxy routes the [[Construct]]
	 * trap rather than the ordinary prototype/constructor bookkeeping below. The
	 * compiler already pushed arg_num args (natural order, argN on top); collect
	 * them into an array and hand off to proxy_construct, which returns at baseline
	 * refs just like the ordinary path. */
	if(var_is_proxy(n->var)) {
		vm->gc.gc_defer++;
		var_t* pargs = var_new_array(vm);
		for(int i = 0; i < arg_num; i++) {
			var_t* a = vm_pop2(vm);
			var_array_add(pargs, (a != NULL) ? a : var_new(vm));
			if(a != NULL) var_unref(a);
		}
		var_array_reverse(pargs);
		obj = proxy_construct(vm, n->var, pargs, n->var);
		var_unref(pargs);
		vm->gc.gc_defer--;
		return obj;
	}

	var_t* protoV = var_get_prototype(n->var);
	obj = var_new_obj(vm, protoV, NULL, NULL);
	var_t* constructor = NULL;

	if(n->var->is_func) { // new object built by function call
		constructor = n->var;
	}
	else {
		constructor = var_find_member_var(protoV, CONSTRUCTOR);
	}

	if(constructor != NULL) {
		/* Protect obj across func_call: for script constructors the only other
		 * reference is env's `this`, which is released when func_call pops env.
		 * Without this guard obj would be freed before we can return it. */
		var_ref(obj);
		/* ES6 `new.target`: expose the constructed class/function to the call.
		 * func_call consumes it (binds into env, clears the field); restore the
		 * previous value afterwards so nested/outer constructions stay correct. */
		var_t* old_nt = vm->new_target;
		vm->new_target = n->var;
		func_call(vm, obj, constructor, arg_num);
		vm->new_target = old_nt;
		var_t* ret = vm_pop2(vm); // no unref: ret carries func_call's push ref
		if(ret != NULL && ret != obj && ret->type == V_OBJECT) {
			// JS semantics: an explicit object returned from a constructor wins.
			obj->refs--; // release protection ref (obj discarded)
			obj = ret;
			obj->refs--; // release func_call's push ref (matches return contract)
		}
		else {
			// Keep the freshly constructed object; discard the return value.
			if(ret != NULL)
				ret->refs--; // release func_call's push ref (ret may == obj for natives)
			obj->refs--;     // release protection ref
		}
	}
	return obj;
}

static int parse_func_name(const char* full, mstr_t* name) {
	/* gen_func_name() encodes a call as "<name>$<argcount>" (suffix present only
	 * when argcount>0). JS identifiers may themselves contain '$' (e.g. jQuery's
	 * `$`, or `$el`/`a$b`), so splitting at the FIRST '$' corrupts them: "$" would
	 * yield an empty name and "$(sel)"->"$$1" would too. The arity suffix is
	 * always a trailing '$' followed by one or more digits and nothing else, so
	 * split at the LAST such '$'; any other '$' belongs to the identifier. */
	int args_num = 0;
	const char* sep = NULL;
	for(const char* p = full; *p != 0; ++p) {
		if(*p != '$' || *(p+1) == 0)
			continue;
		const char* q = p+1;
		while(*q >= '0' && *q <= '9')
			q++;
		if(*q == 0)   // '$' followed by only digits up to end -> arity suffix
			sep = p;
	}
	if(sep != NULL) {
		args_num = atoi(sep+1);
		if(name != NULL)
			mstr_ncpy(name, full, (uint32_t)(sep-full));
	}
	else {
		if(name != NULL)
			mstr_cpy(name, full);
	}
	return args_num;
}

/** create object and try constructor */
static bool do_new(vm_t* vm, const char* full) {
	mstr_t* name = mstr_new("");
	int arg_num = parse_func_name(full, name);

	/* ES6: an arrow function has no [[Construct]]; `new arrow()` is a catchable
	 * TypeError. Detect it before building anything, drop the pushed args, and
	 * throw. Return true so handle_new does not vm_terminate (the throw must be
	 * catchable by an enclosing try/catch). */
	node_t* cn = vm_load_node(vm, name->cstr, false);
	if(cn != NULL && cn->var != NULL && cn->var->is_func) {
		func_t* cf = var_get_func(cn->var);
		if(cf != NULL && cf->is_arrow) {
			int k = arg_num;
			while(k-- > 0)
				vm_pop(vm);
			vm_throw(vm, "arrow function '%s' is not a constructor!", name->cstr);
			mstr_free(name);
			return true;
		}
	}

	var_t* obj = new_obj(vm, name->cstr, arg_num);
	mstr_free(name);

	if(obj == NULL)
		return false;
	vm_push(vm, obj);
	return true;
}

var_t* call_m_func(vm_t* vm, var_t* obj, var_t* func, var_t* args) {
	//push args to stack.
	int arg_num = 0;
	/* `args` (and any hole-filler we create) is held only by this C frame while
	 * the callee runs, so an opportunistic gc in the callee (vm_pop_scope) would
	 * sweep it and the caller's var_unref(args) would become a UAF. Defer gc for
	 * the whole call instead of anchoring args on the value stack: the old
	 * vm_push/vm_pop anchor churned args->refs, and when a caller passed an args
	 * array whose only live reference was the one it was about to release
	 * (refs==0 on entry, e.g. handle_call_spread), the anchor pop freed args and
	 * the caller's later var_unref(args) read freed memory. gc_defer protects
	 * args without touching its refcount, leaving exactly one release (the
	 * caller's). The elements still go on the value stack for func_call. */
	vm->gc.gc_defer++;
	if(args != NULL) {
		// Push arguments onto the stack
		arg_num = var_array_size(args);
		for(int i = arg_num - 1; i >= 0; i--) {
			node_t* node = var_array_get(args, i);
			if(node == NULL || node->var == NULL) //hole/undefined element: node_new(NULL) would strlen(NULL)
				vm_push(vm, var_new(vm));
			else
				vm_push(vm, node->var);
		}
	}

	while(vm->gc.is_doing_gc);
	func_call(vm, obj, func, arg_num);
	var_t* ret = vm_pop2(vm);
	if(ret != NULL && obj == ret)
		ret->refs--;
	vm->gc.gc_defer--;
	return ret;
}

var_t* call_m_func_by_name(vm_t* vm, var_t* obj, const char* func_name, uint32_t arg_num, ... ) {
	va_list args;
    va_start(args, arg_num);
	var_t* func_args = var_new_array(vm);

    for (int i = 0; i < arg_num; i++) {
        var_t* arg = va_arg(args, var_t*);  // 取出void*类型的可变参数
		var_array_add(func_args, arg);
    }
    va_end(args);
	var_array_reverse(func_args);

	if(obj == NULL)
		obj = vm->root;
	node_t* func = var_find_member(obj, func_name);

	if(func == NULL || func->var->is_func == 0) {
		mario_printf("Interrupt function '%s' not defined!\n", func_name);
		return NULL;
	}
	var_t* ret = call_m_func(vm, obj, func->var, func_args);
	var_unref(func_args);
	return ret;
}

/*****************/

var_t* vm_new_class(vm_t* vm, const char* cls) {
	node_t* n = vm_load_node(vm, cls, true);
	if(n == NULL)
		return NULL;
	var_t* cls_var = n->var;
	cls_var->type = V_OBJECT;
	/* A class/constructor is callable (`new X()`), so `typeof X` must report
	 * "function" per spec. is_func stays 0 on purpose: new_obj() dispatches
	 * native-class construction through the prototype's constructor member and
	 * would treat an is_func class var as a plain function object (no func_t),
	 * breaking `new`. is_class is read only by get_typeof(). */
	cls_var->is_class = 1;
	if(var_get_prototype(cls_var) == NULL) {
		var_set_prototype(cls_var, var_new_obj_no_proto(vm, NULL, NULL));
	}

	if(strcmp(cls, "Object") != 0)
		do_extends(vm, cls_var, "Object");
	return cls_var;
}

void vm_terminate(vm_t* vm) {
	while(true) { //clear stack
		if(!vm_pop(vm))
			break;
	}

	while(true) { //clear scopes
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) break;
		vm_pop_scope(vm);
	}
	vm->pc = vm->bc.cindex;
}

static void do_include(vm_t* vm, const char* jsname) {
	if(_load_m_func == NULL)
		return;
	//check if included or not.
	int i;
	for(i=0; i<vm->included.size; i++) {
		mstr_t* jsn = (mstr_t*)array_get(&vm->included, i);
		if(strcmp(jsn->cstr, jsname) == 0)
			return;
	}

	mstr_t* js = _load_m_func(vm, jsname);
	if(js == NULL) {
		mario_printf("Error: include file '%s' not found!\n", jsname);
		return;
	}
	array_add(&vm->included, mstr_new(jsname));

	PC pc = vm->pc;
	vm_load_run(vm, js->cstr);
	mstr_free(js);
	vm->pc = pc;
}

/* Instruction handler function type for table-based dispatch */
typedef void (*instr_handler_t)(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset);

/* Instruction handler functions for table-based dispatch */

static inline void handle_jmp(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm->pc = vm->pc + offset - 1;
}

static inline void handle_jmpb(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm->pc = vm->pc - offset - 1;
}

static inline void handle_njmp(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	if(v->type == V_UNDEF || v->value == NULL || *(int*)(v->value) == 0) {
		if(instr == INSTR_NJMP) 
			vm->pc = vm->pc + offset - 1;
		else
			vm->pc = vm->pc - offset - 1;
	}
	var_unref(v);
}

static inline void handle_load(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	bool loaded = false;
	node_t* node = NULL;
	if(offset == vm->this_strIndex) {
		var_t* thisV = vm_this_in_scopes(vm);
		if(thisV != NULL) {
			vm_push(vm, thisV);
			loaded = true;
		}
		else {
			// this is NULL, push undefined
			vm_push(vm, var_new(vm));
			loaded = true;
		}
	}

	if(!loaded) {
		const char* s = bc_getstr(&vm->bc, offset);
		node = vm_load_node(vm, s, false);
		if(node == NULL) {
			scope_t* sc = vm_get_strict_scope(vm); //check strict mode
			if(sc != NULL) {
				vm_throw(vm, "'%s' undefined!", s);	
				return;
			}
			var_t* var = vm_get_scope_var(vm);
			node = var_add(var, s, NULL);
		}
		vm_push_node(vm, node);
	}

	if(node == NULL || node->var == NULL)
		return;

	if(vm->gen_depth == 0 && vm_get_loop_scope(vm) != NULL) //only cache in loop scope (never in a generator: suspension outlives the cached nodes).
		load_ncache(vm, node, vm->pc-1);
}

static inline void handle_compare(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		if(v1 != NULL) var_unref(v1);
		if(v2 != NULL) var_unref(v2);
		return;
	}
	compare(vm, instr, v1, v2);
	var_unref(v1);
	var_unref(v2);
}

static inline void handle_nil(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	/* Do nothing */
}

static inline void handle_strict(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	scope_t* sc = vm_get_scope(vm);
	if(sc != NULL) {
		sc->is_strict = true;
	}
}

static inline void handle_block(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	scope_t* sc = scope_new(var_new_block(vm));
	sc->is_block = true;
	sc->stack_top = vm->stack_top; //entry height (a block nests inside its func frame's baseline)
	if(instr == INSTR_LOOP) {
		sc->is_loop = true;
		sc->pc_start = vm->pc+1;
		sc->pc = vm->pc+2;
	}
	else if(instr == INSTR_TRY) {
		sc->is_try = true;
		sc->pc = vm->pc+1;
	}
	else if(instr == INSTR_SWITCH) {
		/* Switch scope: sc->pc is the break anchor (a reserved JMP-to-end slot the
		 * compiler emits right after SWITCH). `break` jumps here; `continue` must
		 * NOT stop here (it belongs to an enclosing loop), hence a distinct flag. */
		sc->is_switch = true;
		sc->pc = vm->pc+1;
	}
	vm_push_scope(vm, sc);
}

static inline void handle_block_end(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_pop_scope(vm);
}

static inline void handle_break(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			mario_printf("Error: 'break' not in any loop!\n");
			vm_terminate(vm);
			break;
		}
		if(sc->is_loop || sc->is_switch) {
			vm->pc = sc->pc;
			break;
		}
		vm_pop_scope(vm);
	}
}

static inline void handle_continue(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			mario_printf("Error: 'continue' not in any loop!\n");
			vm_terminate(vm);
			break;
		}
		if(sc->is_loop) {
			vm->pc = sc->pc_start;
			break;
		}
		vm_pop_scope(vm);
	}
}

static inline void handle_cache(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	if(vm->var_cache.size == 0)
		return;
	var_t* v = vm->var_cache.cache[offset];
	vm_push(vm, v);
}

static inline void handle_ncache(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	if(vm->load_ncache.size == 0)
		return;
	node_t* node = vm->load_ncache.cache[offset].node;
	vm_push_node(vm, node);
}

static inline void handle_true(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_push(vm, vm->builtin_vars.var_true);
}

static inline void handle_false(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_push(vm, vm->builtin_vars.var_false);
}

static inline void handle_null(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_push(vm, vm->builtin_vars.var_null);
}

static inline void handle_undef(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = var_new(vm);
	vm_push(vm, v);
}

static inline void handle_pop(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_pop(vm);
}

static inline void handle_neg(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	switch(v->type) {
		case V_INT: {
			int n = *(int*)v->value;
			/* -0 must be a distinct negative zero (Object.is(0,-0) === false),
			 * which no integer lane can represent: negating integer 0 yields the
			 * double -0.0. Other integers box at the narrowest exact width, so
			 * -(-2147483648) promotes to int64 rather than wrapping. */
			if(n == 0)
				vm_push(vm, var_new_float64(vm, -0.0));
			else
				vm_push(vm, box_int_result(vm, -(int64_t)n));
			break;
		}
		case V_INT64: {
			int64_t n = *(int64_t*)v->value;
			if(n == 0)
				vm_push(vm, var_new_float64(vm, -0.0));
			else
				vm_push(vm, box_int_result(vm, -n));
			break;
		}
		case V_FLOAT:
			vm_push(vm, var_new_float(vm, -(*(float*)v->value)));
			break;
		case V_FLOAT64:
			vm_push(vm, var_new_float64(vm, -(*(double*)v->value)));
			break;
		case V_BIGINT:
			vm_push(vm, var_new_bigint(vm, bn_neg((bignum_t*)v->value)));
			break;
		default:
			/* -"x" / -undefined etc.: JS yields NaN. Push it so the value stack
			 * stays balanced (the old code pushed nothing for non-numerics). */
			vm_push(vm, var_new_float64(vm, NAN));
			break;
	}
	var_unref(v);
}

/* Bitwise NOT `~x`: ToInt32(x) then invert. A BigInt operand yields -(x+1)
 * (still a BigInt). Non-numeric operands coerce like JS ToNumber: bool/null ->
 * 0/1, a numeric string -> its value, anything else (undefined, object, junk
 * string) -> NaN, and ToInt32(NaN) == 0, so ~NaN == -1. */
static inline void handle_bnot(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new_int(vm, -1)); // ~undefined == ~0 == -1
		return;
	}
	if(v->type == V_BIGINT) {
		bignum_t* one = bn_from_int64(1);
		bignum_t* s = bn_add((bignum_t*)v->value, one); // x + 1
		bn_free(one);
		bignum_t* r = bn_neg(s);                        // -(x + 1)
		bn_free(s);
		var_unref(v);
		vm_push(vm, var_new_bigint(vm, r));
		return;
	}
	double d;
	switch(v->type) {
		case V_INT:     d = (double)(*(int*)v->value); break;
		case V_INT64:   d = (double)(*(int64_t*)v->value); break;
		case V_FLOAT:   d = (double)(*(float*)v->value); break;
		case V_FLOAT64: d = *(double*)v->value; break;
		case V_BOOL:    d = var_get_bool(v) ? 1.0 : 0.0; break;
		case V_NULL:    d = 0.0; break;
		case V_STRING: {
			const char* s = var_get_str(v);
			while(s != NULL && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
			char* end = NULL;
			d = (s != NULL && *s != 0) ? strtod(s, &end) : 0.0;
			if(end != NULL) {
				while(*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
				if(*end != 0) d = NAN; // trailing junk -> NaN
			}
			break;
		}
		default:        d = NAN; break; // V_UNDEF, V_OBJECT
	}
	int32_t a;
	if(isnan(d) || isinf(d)) {
		a = 0;
	} else {
		double m = fmod(trunc(d), 4294967296.0);
		int64_t i = (int64_t)m;
		if(i < 0) i += 4294967296LL;
		a = (int32_t)(uint32_t)i;
	}
	var_unref(v);
	vm_push(vm, var_new_int(vm, ~a));
}

/* Box a double as the narrowest exact numeric var: a whole number within the
 * safe-integer range becomes V_INT/V_INT64, anything else (fractional, NaN,
 * Infinity, or beyond 2^53) becomes the canonical V_FLOAT64. */
static inline var_t* var_from_double(vm_t* vm, double d) {
	if(!isnan(d) && !isinf(d) && d == floor(d) && fabs(d) < 9223372036854775808.0)
		return box_int_result(vm, (int64_t)d);
	return var_new_float64(vm, d);
}

/* Unary plus `+x`: ToNumber. Objects fall back to NaN until Symbol.toPrimitive
 * / valueOf coercion is wired up (see section 12). */
static inline void handle_pos(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	switch(v->type) {
		case V_INT:
			vm_push(vm, var_new_int(vm, *(int*)v->value));
			break;
		case V_INT64:
			vm_push(vm, var_new_int64(vm, *(int64_t*)v->value));
			break;
		case V_FLOAT:
		case V_FLOAT64:
			vm_push(vm, var_new_float64(vm, var_get_float64(v)));
			break;
		case V_BIGINT:
			/* Unary `+bigint` is a TypeError in JS: no implicit BigInt->Number. */
			vm_throw_type(vm, "TypeError", "Cannot convert a BigInt value to a number");
			break;
		case V_BOOL:
			vm_push(vm, var_new_int(vm, var_get_bool(v) ? 1 : 0));
			break;
		case V_NULL:
			vm_push(vm, var_new_int(vm, 0));
			break;
		case V_STRING: {
			const char* s = var_get_str(v);
			while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
			if(*s == 0) {
				vm_push(vm, var_new_int(vm, 0));
				break;
			}
			char* end = NULL;
			double d = strtod(s, &end);
			while(end != NULL && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
			if(end != NULL && *end != 0)
				vm_push(vm, var_new_float64(vm, NAN)); // trailing junk -> NaN
			else
				vm_push(vm, var_from_double(vm, d));
			break;
		}
		default: // V_UNDEF, V_OBJECT
			if(v->type == V_OBJECT) {
				/* ES6 Symbol.toPrimitive(number) / numeric coercion. */
				var_t* p = vm_to_primitive(vm, v, "number");
				if(p != NULL) {
					if(p->type == V_INT)
						vm_push(vm, var_new_int(vm, var_get_int(p)));
					else if(p->type == V_INT64)
						vm_push(vm, var_new_int64(vm, var_get_int64(p)));
					else if(p->type == V_FLOAT || p->type == V_FLOAT64)
						vm_push(vm, var_new_float64(vm, var_get_float64(p)));
					else if(p->type == V_BOOL)
						vm_push(vm, var_new_int(vm, var_get_bool(p) ? 1 : 0));
					else if(p->type == V_STRING) {
						const char* ps = var_get_str(p);
						char* end = NULL;
						double d = (ps != NULL && *ps != 0) ? strtod(ps, &end) : 0.0;
						if(end != NULL && *end != 0)
							vm_push(vm, var_new_float64(vm, NAN));
						else
							vm_push(vm, var_from_double(vm, d));
					}
					else
						vm_push(vm, var_new_float64(vm, NAN));
					var_unref(p);
					var_unref(v);
					return;
				}
			}
			vm_push(vm, var_new_float64(vm, NAN));
			break;
	}
	var_unref(v);
}

static inline void handle_not(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	/* `!x` is the negated JS ToBoolean. var_truthy() handles every type safely;
	 * the old `*(int*)v->value == 0` mis-read the wider numeric buffers (a small
	 * double has an all-zero low word) and dereferenced NULL for null/empty. */
	bool i = !var_truthy(v);
	var_unref(v);
	vm_push(vm, i ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
}

static inline void handle_logic(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	/* JS `&&`/`||` yield one of the OPERANDS (not a boolean) and accept any value
	 * type. The old code did `*(int*)v->value`: a NULL deref (segfault) on
	 * undefined/null and garbage on strings/objects, and it pushed true/false,
	 * which broke `x = a || default`. Mirror handle_logic_assign(): use the real
	 * ToBoolean helper var_truthy() and push the deciding operand. NOTE: logic()
	 * in the compiler evaluates both sides unconditionally, so this is still not
	 * short-circuit. */
	vm->gc.gc_defer++;
	bool b1 = var_truthy(v1);
	var_t* res;
	if(instr == INSTR_AAND)
		res = b1 ? v2 : v1; // a && b -> a when a is falsy, else b
	else
		res = b1 ? v1 : v2; // a || b -> a when a is truthy, else b
	if(res == NULL)
		res = var_new(vm);
	vm_push(vm, res);
	var_unref(v1);
	var_unref(v2);
	vm->gc.gc_defer--;
}

static inline void handle_math(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	node_t* n = vm_peek_node(vm); //the lvalue binding, used by the compound assignments.
	var_t* v1 = vm_pop2(vm);
	/* Popping leaves v1/v2 - and the result math_op() builds - invisible to gc(),
	 * and math_result()'s node_replace() drops the lvalue's old var, which can
	 * start a collection that sweeps them. See vm_step_op(). */
	vm->gc.gc_defer++;
	/* Accessor compound assign (`obj.x += v`): the compiler retargeted the member
	 * fetch to GETW, so do_get(for_write) pushed [obj, accessor_node] and v1 is the
	 * accessor var itself, not a value. Read the current value through the getter,
	 * compute, then write through the setter (a read-only accessor drops the write,
	 * exactly as handle_asign does). The result stays on the stack as the
	 * expression's value. */
	if(n != NULL && var_is_accessor(n->var)) {
		var_unref(v1);                        // release the popped ref on the accessor var
		var_t* obj = vm_pop2(vm);             // the object GETW pushed beneath the node
		var_t* getter = var_accessor_getter(n->var);
		var_t* cur = NULL;
		if(getter != NULL) {
			var_ref(getter);
			func_call(vm, obj, getter, 0);    // pushes the getter's return value
			cur = vm_pop2(vm);
			var_unref(getter);
		}
		if(cur == NULL) cur = var_new(vm);
		math_op(vm, instr, cur, v2, NULL);    // compute with no node write-back; pushes res
		var_t* res = vm_peek_var(vm);         // borrow the result for the setter argument
		var_t* setter = var_accessor_setter(n->var);
		if(setter != NULL && res != NULL) {
			var_ref(setter); var_ref(res);
			vm_push(vm, res);                 // the setter's argument
			func_call(vm, obj, setter, 1);
			vm_pop(vm);                       // discard the setter's return value
			var_unref(setter); var_unref(res);
		}
		if(obj != NULL) var_unref(obj);
		var_unref(cur);
		var_unref(v2);
		vm->gc.gc_defer--;
		return;
	}
	/* Proxy compound assignment (`p.x += v`): the @@proxyslot placeholder is not the
	 * real current value - resolve it through the get trap now (spec order: get, then
	 * set) and use it as the left operand. A simple `p.x = v` never reaches here, so
	 * it still fires the set trap alone. */
	if(is_compound_assign(instr) && is_proxy_slot(n)) {
		var_unref(v1);                        // discard the placeholder's stack ref
		var_t* p = proxy_slot_obj(n);
		var_t* k = proxy_slot_key(n);
		v1 = proxy_get(vm, p, k, p);          // refs == baseline
		var_ref(v1);                          // balance the trailing var_unref(v1)
	}
	math_op(vm, instr, v1, v2, n);
	var_unref(v1);
	var_unref(v2);
	vm->gc.gc_defer--;
}

/* Build a NEW var holding `v` stepped by `step`. The old var is never written to
 * in place - see math_result() for why that would corrupt aliases and cached
 * integer literals. */
static inline var_t* var_step(vm_t* vm, var_t* v, int step) {
	switch(v->type) {
		case V_FLOAT:   return var_new_float(vm, *(float*)v->value + (float)step);
		case V_FLOAT64: return var_new_float64(vm, *(double*)v->value + (double)step);
		case V_INT64:   return box_int_result(vm, *(int64_t*)v->value + (int64_t)step);
		case V_BIGINT: {
			/* `x++` / `x--` on a BigInt stays a BigInt (step is +/-1). */
			bignum_t* s = bn_from_int64(step);
			bignum_t* r = bn_add((bignum_t*)v->value, s);
			bn_free(s);
			return var_new_bigint(vm, r);
		}
		default:        return box_int_result(vm, (int64_t)(*(int*)v->value) + (int64_t)step); // V_INT
	}
}

/* Shared body of ++/--. `n` is the binding being stepped (NULL when the operand
 * is an rvalue); a prefix op yields the new value, a postfix op the old one.
 * The INSTR_POP peephole is preserved verbatim: when the result is discarded the
 * following POP becomes a NIL and this instruction is flagged so later passes
 * skip it. */
static inline void vm_step_op(vm_t* vm, PC ins, node_t* n, var_t* v, int step, bool prefix) {
	register PC* code = vm->bc.code_buf;
	/* nv, res and v are held only by C locals from here on, so gc() can not see
	 * them: a var that is neither on the value stack nor reachable from the root
	 * is swept, and gc_free_free_vars() then hands its memory back. Every
	 * var_unref() below can start a collection, so keep the collector out until
	 * the result is back on the stack (the guard func_call() uses for env/args). */
	vm->gc.gc_defer++;
	/* Accessor `obj.x++` / `++obj.x`: the compiler retargeted the member fetch to
	 * GETW, so do_get(for_write) pushed [obj, accessor_node] and v is the accessor
	 * var, not a number. Read the current value through the getter, step it and
	 * write it back through the setter; a non-numeric getter result is left
	 * unchanged with no write, mirroring handle_step()'s plain path. */
	if(n != NULL && var_is_accessor(n->var)) {
		var_unref(v);                          // release the popped ref on the accessor var
		var_t* obj = vm_pop2(vm);              // the object GETW pushed beneath the node
		var_t* getter = var_accessor_getter(n->var);
		var_t* cur = NULL;
		if(getter != NULL) {
			var_ref(getter);
			func_call(vm, obj, getter, 0);     // pushes the getter's return value
			cur = vm_pop2(vm);
			var_unref(getter);
		}
		if(cur == NULL) cur = var_new(vm);
		bool steppable = (cur->type == V_INT || cur->type == V_FLOAT ||
		                  cur->type == V_INT64 || cur->type == V_FLOAT64 ||
		                  cur->type == V_BIGINT);
		var_t* nv = steppable ? var_step(vm, cur, step) : cur; // distinct var only when steppable
		if(steppable) var_ref(nv);
		var_t* res = prefix ? nv : cur;
		var_ref(res);
		if(steppable) {
			var_t* setter = var_accessor_setter(n->var);
			if(setter != NULL) {
				var_ref(setter);
				vm_push(vm, nv);               // the setter's argument
				func_call(vm, obj, setter, 1);
				vm_pop(vm);                    // discard the setter's return value
				var_unref(setter);
			}
		}
		if((ins & INSTR_OPT_CACHE) == 0) {
			if(OP(code[vm->pc]) != INSTR_POP)
				vm_push(vm, res);
			else {
				code[vm->pc] = INSTR_NIL;
				code[vm->pc-1] |= INSTR_OPT_CACHE;
			}
		}
		else {
			vm->pc++;
		}
		var_unref(res);
		if(steppable) var_unref(nv);
		var_unref(cur);
		if(obj != NULL) var_unref(obj);
		vm->gc.gc_defer--;
		return;
	}
	/* Proxy `p.x++` / `++p.x`: the @@proxyslot placeholder is not the real current
	 * value - resolve it through the get trap before stepping (spec order: get, then
	 * set). The trailing var_unref(v) balances the reference taken here. */
	if(n != NULL && is_proxy_slot(n)) {
		var_unref(v);                          // discard the placeholder's stack ref
		var_t* p = proxy_slot_obj(n);
		var_t* k = proxy_slot_key(n);
		v = proxy_get(vm, p, k, p);            // refs == baseline
		var_ref(v);                            // our own ref, dropped by var_unref(v) below
	}
	var_t* nv = var_step(vm, v, step);
	var_ref(nv); //our own reference to nv, dropped at the end (as handle_asign() does).
	var_t* res = prefix ? nv : v;
	var_ref(res); //keep the result alive: node_replace() releases the node's old var.
	if(n != NULL) {
		/* A synthetic @@taslot (`ta[i]++`): write the stepped value into the
		 * buffer and free the sentinel; node_free releases the node's own ref on
		 * the decoded element exactly as node_replace's var_unref(old) would. A
		 * @@proxyslot (`p.x++`) drives the proxy set trap and is freed the same way. */
		if(ta_slot_write(vm, n, nv) || proxy_slot_write(vm, n, nv))
			node_free(n);
		else
			node_replace(n, nv); //write the new value back through the binding.
	}

	if((ins & INSTR_OPT_CACHE) == 0) {
		if(OP(code[vm->pc]) != INSTR_POP) {
			vm_push(vm, res);
		}
		else {
			code[vm->pc] = INSTR_NIL;
			code[vm->pc-1] |= INSTR_OPT_CACHE;
		}
	}
	else {
		vm->pc++;
	}
	var_unref(res);
	var_unref(nv); //for a prefix op nv IS res, so it must outlive the push above.
	var_unref(v);
	vm->gc.gc_defer--;
}

/* Only real numbers can be stepped; anything else yields the operand unchanged,
 * as before. Note the old code tested `v->value != NULL`, which treated a string
 * buffer as an int array and incremented its first four bytes. */
static inline void handle_step(vm_t* vm, PC ins, node_t* n, var_t* v, int step, bool prefix) {
	/* An accessor target (`obj.x++`) reaches here as its accessor var, and a proxy
	 * `@@proxyslot` sentinel (`p.x++`) as its lazy undefined placeholder - neither
	 * is a number, so the plain guard below would drop the write. Route both to
	 * vm_step_op, which resolves the getter/setter or the get/set traps. (A
	 * TypedArray's @@taslot already carries the decoded element, so it is numeric
	 * and falls through to the normal path.) See vm_step_op()'s branches. */
	if(n != NULL && (var_is_accessor(n->var) || is_proxy_slot(n))) {
		vm_step_op(vm, ins, n, v, step, prefix);
		return;
	}
	if(v->type == V_INT || v->type == V_FLOAT ||
	   v->type == V_INT64 || v->type == V_FLOAT64) {
		vm_step_op(vm, ins, n, v, step, prefix);
		return;
	}
	vm_push(vm, v);
	var_unref(v);
}

static inline void handle_mminus_pre(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	node_t* n = vm_peek_node(vm); //the binding to write the stepped value back to.
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	handle_step(vm, ins, n, v, -1, true);
}

static inline void handle_mminus(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	node_t* n = vm_peek_node(vm); //the binding to write the stepped value back to.
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	handle_step(vm, ins, n, v, -1, false);
}

static inline void handle_pplus_pre(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	node_t* n = vm_peek_node(vm); //the binding to write the stepped value back to.
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	handle_step(vm, ins, n, v, 1, true);
}

static inline void handle_pplus(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	node_t* n = vm_peek_node(vm); //the binding to write the stepped value back to.
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	handle_step(vm, ins, n, v, 1, false);
}

/* ES6 `return { inc(){...}, get(){...} }`: the methods of a returned object must
 * close over the returning function's scope, just like a directly returned
 * function does (func_set_closure). Propagate the captured scope to each own
 * function member that does not already have a closure. */
typedef struct { var_t* scope_var; func_t* scope_func; } closure_prop_t;
static void propagate_closure_cb(const char* key, void* value, void* user_data) {
	(void)key;
	closure_prop_t* cp = (closure_prop_t*)user_data;
	node_t* node = (node_t*)value;
	if(node == NULL || node->var == NULL)
		return;
	func_t* f = var_get_func(node->var);
	if(f != NULL && f->closure.var == NULL) {
		f->closure.var = var_ref(cp->scope_var);
		func_bind_closure_func(f, cp->scope_func);
	}
}

static inline void handle_return(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* ret = NULL;
	if(instr == INSTR_RETURN) {
		vm_push(vm, var_new(vm));
	}
	else {
		ret = vm_pop2(vm);
		if(ret != NULL) {
			vm_push(vm, ret);
			var_unref(ret);
		}
		else {
			vm_push(vm, var_new(vm));
		}
	}

	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) break;
		if(sc->is_func) {
			if(ret != NULL) {
				func_set_closure(ret, sc->var, sc->func);
				/* Returning an object: bind its freshly-defined methods to this scope. */
				if(ret->type == V_OBJECT && !ret->is_func) {
					closure_prop_t cp;
					cp.scope_var = sc->var;
					cp.scope_func = sc->func;
					hash_map_iterate(&ret->children, propagate_closure_cb, &cp);
				}
			}

			vm->pc = sc->pc;
			vm_pop_scope(vm);
			break;
		}
		vm_pop_scope(vm);
	}
}

static inline void handle_var(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	/* ES5 `var` is function-scoped, not block-scoped. Hoist the declaration to
	 * the nearest enclosing function scope (or the global scope), skipping any
	 * block/loop/try scopes. Otherwise a `var` declared inside a block would be
	 * destroyed when that block scope pops, deviating from JS semantics
	 * (e.g. `for(var i=0;..){var x=i;} print(x)` must see x afterwards). */
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL && !sc->is_func)
		sc = sc->prev;
	var_t* v = (sc != NULL) ? sc->var : vm->root;
	if(v == NULL)
		return;
	if(var_find_own_member(v, s) == NULL)
		var_add(v, s, NULL);
}

static inline void handle_const(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = vm_get_scope_var(vm);
	node_t *node = var_find_own_member(v, s);
	if(node != NULL) {
		mario_debug("Error: let '%s' has already existed!\n", s);
		vm_throw(vm, "let '%s' has already existed!", s);
	}
	else {
		node = var_add(v, s, NULL);
		if(node != NULL && instr == INSTR_CONST)
			node->be_const = true;
	}
}

static inline void handle_int(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	var_t* v = var_new_int(vm, (int)code[vm->pc++]);
	if(try_var_cache(vm, &code[vm->pc-2], v))
		code[vm->pc-1] = INSTR_NIL;
	vm_push(vm, v);
}

static inline void handle_int_s(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	var_t* v = var_new_int(vm, offset);
	try_var_cache(vm, &code[vm->pc-1], v);
	vm_push(vm, v);
}

static inline void handle_float(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	var_t* v = var_new_float(vm, *(float*)(&code[vm->pc++]));
	if(try_var_cache(vm, &code[vm->pc-2], v))
		code[vm->pc-1] = INSTR_NIL;
	vm_push(vm, v);
}

static inline void handle_int64(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	/* 2-word payload reassembled symmetrically with bc_gen_str's emit. */
	uint32_t words[2];
	words[0] = code[vm->pc];
	words[1] = code[vm->pc+1];
	vm->pc += 2;
	int64_t ll;
	memcpy(&ll, words, sizeof(ll));
	var_t* v = var_new_int64(vm, ll);
	if(try_var_cache(vm, &code[vm->pc-3], v)) {
		code[vm->pc-2] = INSTR_NIL;
		code[vm->pc-1] = INSTR_NIL;
	}
	vm_push(vm, v);
}

static inline void handle_float64(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	uint32_t words[2];
	words[0] = code[vm->pc];
	words[1] = code[vm->pc+1];
	vm->pc += 2;
	double dd;
	memcpy(&dd, words, sizeof(dd));
	var_t* v = var_new_float64(vm, dd);
	if(try_var_cache(vm, &code[vm->pc-3], v)) {
		code[vm->pc-2] = INSTR_NIL;
		code[vm->pc-1] = INSTR_NIL;
	}
	vm_push(vm, v);
}

static inline void handle_str(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = var_new_str(vm, s);
	try_var_cache(vm, &code[vm->pc-1], v);
	vm_push(vm, v);
}

/* INSTR_BIGINT: the pooled string payload holds the literal's digits (with an
 * optional 0x/0b/0o prefix); parse them into a bignum and push a V_BIGINT. Like
 * handle_str, the literal is cache-eligible so a hot `1n` is not re-parsed. */
static inline void handle_bigint(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	const char* s = bc_getstr(&vm->bc, offset);
	bignum_t* b = bn_from_string(s, 0);
	var_t* v = var_new_bigint(vm, b);
	try_var_cache(vm, &code[vm->pc-1], v);
	vm_push(vm, v);
}

static inline void handle_asign(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	var_t* v = vm_pop2(vm);
	node_t* n = vm_pop2node(vm);
	/* Popping takes v off the value stack, so from here on it is held only by a C
	 * local and gc() can not see it - and every var_unref() below can start a
	 * collection, whose gc_free_free_vars() hands v's memory back while we still
	 * need it (node_replace() would then var_ref() freed memory). Keep the
	 * collector out until v is back on the stack or released. See vm_step_op(). */
	vm->gc.gc_defer++;
	if(n == NULL) {
		mario_debug("Error: Can not find an assignable target!\n");
		var_unref(v);
		vm->gc.gc_defer--;
		return;
	}

	/* Synthetic @@taslot target (`ta[i] = v`): encode v into the TypedArray's
	 * buffer, free the sentinel, and yield v as the assignment expression's
	 * value. vm_pop2node left the decoded element holding its stack reference, so
	 * release it here (mirroring the var_unref(n->var) on the normal path);
	 * node_free then releases the node's own reference and frees the sentinel. */
	if(ta_slot_write(vm, n, v) || proxy_slot_write(vm, n, v)) {
		var_unref(n->var);
		node_free(n);
		if((ins & INSTR_OPT_CACHE) == 0) {
			if(OP(code[vm->pc]) != INSTR_POP) {
				vm_push(vm, v);
			}
			else {
				code[vm->pc] = INSTR_NIL;
				code[vm->pc-1] |= INSTR_OPT_CACHE;
			}
		}
		else {
			vm->pc++;
		}
		var_unref(v);
		vm->gc.gc_defer--;
		return;
	}

	/* ES6 accessor: the target node holds a getter/setter rather than a plain
	 * value. do_get(for_write) pushed the object beneath the node, so pop it as
	 * the setter's `this`. A read-only accessor silently ignores the write. */
	if(var_is_accessor(n->var)) {
		var_t* obj = vm_pop2(vm);
		var_t* setter = var_accessor_setter(n->var);
		var_unref(n->var); //release the node's stack reference
		if(setter != NULL) {
			var_ref(setter);   //protect the setter across the call
			vm_push(vm, v);    //the assigned value is the setter's argument
			func_call(vm, obj, setter, 1);
			vm_pop(vm);        //discard the setter's return value
			var_unref(setter);
		}
		if(obj != NULL)
			var_unref(obj);    //release the object pushed by do_get(for_write)
		vm_push(vm, v);        //an assignment expression yields the RHS value
		var_unref(v);
		vm->gc.gc_defer--;
		return;
	}

	bool modi = (!n->be_const || n->var->type == V_UNDEF);
	var_unref(n->var);
	if(modi) {
		node_replace(n, v);
	}
	else {
		mario_debug("Error: Can not change a const variable: '%s'!\n", n->name);
		vm_throw(vm, "can not change a const variable: '%s'!", n->name);
		var_unref(v);
		vm->gc.gc_defer--;
		return;
	}

	if((ins & INSTR_OPT_CACHE) == 0) {
		if(OP(code[vm->pc]) != INSTR_POP) {
			vm_push(vm, n->var);
		}
		else {
			code[vm->pc] = INSTR_NIL;
			code[vm->pc-1] |= INSTR_OPT_CACHE;
		}
	}
	else {
		vm->pc++;
	}
	var_unref(v);
	vm->gc.gc_defer--;
}

static inline void handle_get(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	do_get(vm, v, s, false);
	var_unref(v);
}

/* Member fetch used as an assignment target (`obj.prop = v`). Identical to
 * handle_get for plain properties; for an accessor it leaves the object and the
 * node on the stack so handle_asign can invoke the setter with the right `this`. */
static inline void handle_getw(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}
	do_get(vm, v, s, true);
	var_unref(v);
}

/* ES2020 optional chaining `base?.member`. The base is on the stack: if it is
 * nullish the whole access collapses to undefined (replace the top), otherwise
 * it is a plain member fetch identical to INSTR_GET. Because each `?.` link
 * re-inspects the value the previous link left, a chain short-circuits once any
 * link is nullish. */
static inline void handle_opt_get(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* top = vm_peek_var(vm);
	if(var_is_nullish(top)) {
		vm_pop(vm);                // drop the nullish base (releases its push ref)
		vm_push(vm, var_new(vm));  // undefined
		return;
	}
	var_t* v = vm_pop2(vm);
	do_get(vm, v, s, false);
	var_unref(v);
}

/* ES2020 `a ?? b` short-circuit jump. The LHS is on the stack: if it is
 * non-nullish, jump past the RHS keeping the LHS as the result; otherwise pop
 * the nullish LHS and fall through so the RHS is evaluated. */
static inline void handle_nullish(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* top = vm_peek_var(vm);
	if(!var_is_nullish(top)) {
		vm->pc = vm->pc + offset - 1; // keep LHS, skip RHS
	} else {
		vm_pop(vm); // drop nullish LHS, evaluate RHS next
	}
}

/* Short-circuit `&&` / `||`. Mirrors handle_nullish(): the LHS is already on the
 * stack. If it decides the result (`||` truthy / `&&` falsy) keep it and jump
 * past the RHS; otherwise pop it and fall through to evaluate the RHS, whose
 * value becomes the result. Unlike the old INSTR_AAND/INSTR_OOR path this never
 * evaluates the RHS unnecessarily, so guards like `el && el.x` are safe. */
static inline void handle_logic_sc(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* top = vm_peek_var(vm);
	bool truthy = var_truthy(top);
	if((instr == INSTR_SCOR && truthy) || (instr == INSTR_SCAND && !truthy)) {
		vm->pc = vm->pc + offset - 1; // keep LHS, skip RHS
	} else {
		vm_pop(vm); // drop LHS, evaluate RHS next
	}
}

/* ES2021 logical assignment `||= &&= ??=`. Mirrors the arithmetic compound
 * assignment path (handle_math/math_result): the RHS is on top, the lvalue
 * binding beneath it. Pick the result per the operator, install it through the
 * node and leave it as the expression value. */
static inline void handle_logic_assign(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);      // RHS
	node_t* n = vm_peek_node(vm); // lvalue binding
	var_t* v1 = vm_pop2(vm);      // current value
	vm->gc.gc_defer++;
	var_t* res;
	if(instr == INSTR_OREQ)
		res = var_truthy(v1) ? v1 : v2;
	else if(instr == INSTR_ANDEQ)
		res = var_truthy(v1) ? v2 : v1;
	else // INSTR_NULLISHEQ
		res = var_is_nullish(v1) ? v2 : v1;
	if(res == NULL)
		res = var_new(vm);
	if(n != NULL)
		node_replace(n, res); // the node takes its own reference to res
	vm_push(vm, res);
	var_unref(v1);
	var_unref(v2);
	vm->gc.gc_defer--;
}

static inline void handle_new(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	if(!do_new(vm, s))
		vm_terminate(vm);
}

static inline void handle_call(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* func = NULL;
	var_t* obj = NULL;
	bool unrefObj = false;
	const char* s = bc_getstr(&vm->bc, offset);
	mstr_t* name = mstr_new("");
	int arg_num = parse_func_name(s, name);
	var_t* sc_var = vm_get_scope_var(vm);

	if(instr == INSTR_CALLO) {
		obj = vm_stack_pick(vm, arg_num+1);
		unrefObj = true;
		/* ES6 `super.method()`: the receiver picked off the stack is the home
		 * object's [[Prototype]] (where the method is looked up), but `this`
		 * inside the method must stay the current instance. Detect the super
		 * binding, resolve the method on the prototype, and swap the receiver
		 * for the real `this` (keeping the ref/unref contract balanced). */
		var_t* sup = vm_super_in_scopes(vm);
		if(sup != NULL && obj == sup) {
			func = find_func(vm, sup, name->cstr);
			var_t* realthis = vm_this_in_scopes(vm);
			if(realthis != NULL) {
				var_unref(obj);
				obj = realthis;
				var_ref(obj);
			}
		}
	}
	else {
		obj = vm_this_in_scopes(vm);
		func = find_func(vm, sc_var, name->cstr);
	}

	if(func == NULL && obj != NULL)
		func = find_func(vm, obj, name->cstr);

	if(func != NULL && !func->is_func && !var_is_proxy(func)) {
		var_t* constr = var_find_own_member_var(func, CONSTRUCTOR);
		if(constr == NULL) {
			var_t* protoV = var_get_prototype(func);
			if(protoV != NULL)
				func = var_find_own_member_var(protoV, CONSTRUCTOR);
			else
				func = NULL;
		}
		else {
			func = constr;
		}
	}

	if(func != NULL) {
		func_call(vm, obj, func, arg_num);
	}
	else {
		vm->gc.gc_defer++; //obj is a bare C pointer while the args are popped and the throw unwinds
		while(arg_num > 0) {
			vm_pop(vm);
			arg_num--;
		}
		vm_push(vm, var_new(vm));
		mario_debug("Error: can not find function '%s'!\n", name->cstr);
		vm_throw(vm, "can not find function '%s'!", name->cstr);
		vm->gc.gc_defer--;
	}
	mstr_free(name);

	if(unrefObj && obj != NULL)
		var_unref(obj);
}

/* ES6 call-by-value: `(function(){...})()`, `(expr)(args)`, `f()()`. The
 * callable value sits on the stack just below its arg_num arguments, exactly
 * where INSTR_CALLO keeps its receiver. Pick it off (removing that stack slot),
 * call it with the args above it, then release the ref the value-stack held.
 * The operand encodes only the arity as "$n" (an empty function name). */
static inline void handle_callx(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	mstr_t* name = mstr_new("");
	int arg_num = parse_func_name(s, name);
	mstr_free(name);

	var_t* func = vm_stack_pick(vm, arg_num + 1);
	var_t* obj = vm_this_in_scopes(vm);

	if(func != NULL && (func->is_func || var_is_callable(func))) {
		func_call(vm, obj, func, arg_num);
	}
	else {
		vm->gc.gc_defer++; //func is a bare C pointer while the args are popped and the throw unwinds
		while(arg_num > 0) {
			vm_pop(vm);
			arg_num--;
		}
		vm_push(vm, var_new(vm));
		vm_throw(vm, "value is not a function!");
		vm->gc.gc_defer--;
	}

	if(func != NULL)
		var_unref(func); // release the ref the value-stack slot held
}

/* ES6 `obj[key](args)`: the stack holds (top-down) argN..arg1, func, receiver
 * (INSTR_ARRAY_AT_M left the receiver beneath the member). Pick the function
 * and the receiver, then invoke with this=receiver. The operand encodes only
 * the arity as "$n" (an empty function name), like INSTR_CALLX. */
static inline void handle_callxo(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	(void)ins; (void)instr;
	const char* s = bc_getstr(&vm->bc, offset);
	mstr_t* name = mstr_new("");
	int arg_num = parse_func_name(s, name);
	mstr_free(name);

	var_t* func = vm_stack_pick(vm, arg_num + 1); /* removes the func slot */
	var_t* obj  = vm_stack_pick(vm, arg_num + 1); /* receiver now at that depth */

	if(func != NULL && (func->is_func || var_is_callable(func))) {
		func_call(vm, obj, func, arg_num);
	}
	else {
		vm->gc.gc_defer++; //func/obj are bare C pointers while the args are popped and the throw unwinds
		while(arg_num > 0) {
			vm_pop(vm);
			arg_num--;
		}
		vm_push(vm, var_new(vm));
		vm_throw(vm, "value is not a function!");
		vm->gc.gc_defer--;
	}

	vm->gc.gc_defer++; //the first unref may gc-sweep the second target
	if(func != NULL)
		var_unref(func);
	if(obj != NULL)
		var_unref(obj);
	vm->gc.gc_defer--;
}

/* ES6 tagged template: after the compiler builds the cooked `strings` array and
 * the `raw` array (both pushed above the tag callable), this pops rawArr and
 * stringsArr, attaches stringsArr.raw = rawArr, and pushes stringsArr back so
 * it becomes the first argument of the tag call. */
static inline void handle_tag_raw(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* raw = vm_pop2(vm);
	var_t* strings = vm_pop2(vm);
	if(strings != NULL && raw != NULL)
		var_add(strings, "raw", raw);
	if(raw != NULL)
		var_unref(raw);
	if(strings == NULL)
		strings = var_new(vm);
	vm_push(vm, strings);
	var_unref(strings);
}

static inline void handle_member(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = (instr == INSTR_MEMBER ? "" : bc_getstr(&vm->bc, offset));
	var_t* v = vm_pop2(vm);
	if(v == NULL)
		v = var_new(vm);

	var_t *var = vm_get_scope_var(vm);
	if(var->is_array) {
		var_array_add(var, v);
	}
	else {
		if(v->is_func) {
			func_t* func = (func_t*)v->value;
			func->owner = var;
		}
		/* ES6 accessor: a getter and a setter share one property name, so merge
		 * the second one into the existing accessor instead of overwriting it. */
		if(instr != INSTR_MEMBER && var_is_accessor(v)) {
			node_t* ex = var_find_own_member(var, s);
			if(ex != NULL && var_is_accessor(ex->var)) {
				merge_accessor(ex, v);
				var_unref(v);
				return;
			}
		}
		node_t* n = var_add(var, s, v);
		/* ES6: methods defined in a class body live on the prototype (or on the
		 * constructor for statics) and are non-enumerable, unlike object-literal
		 * members. A class-body scope is identified by class_var (set in
		 * handle_class); object literals (handle_obj) leave it NULL. */
		if(n != NULL && v->is_func) {
			scope_t* sc = vm_get_scope(vm);
			if(sc != NULL && sc->class_var != NULL)
				n->be_unenumerable = 1;
		}
	}
	var_unref(v);
}

/* ES6 closures: capture the nearest enclosing function scope at DEFINITION time
 * so inner functions and object/class methods see outer variables. The legacy
 * path only captured a directly `return`ed function (handle_return), which left
 * object-literal methods and stored functions without their lexical scope.
 * Definition-time capture is also the correct scope; handle_return's capture is
 * now guarded to skip when this already ran. */
static void vm_capture_closure(vm_t* vm, var_t* funcv) {
	func_t* f = var_get_func(funcv);
	if(f == NULL || f->closure.var != NULL)
		return;
	scope_t* sc = vm_get_scope(vm);
	while(sc != NULL && !sc->is_func)
		sc = sc->prev;
	if(sc == NULL || sc->var == NULL || sc->func == NULL)
		return;
	f->closure.var = var_ref(sc->var);
	func_bind_closure_func(f, sc->func);
}

static inline void handle_func(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	int regular = FUNC_REGULAR;
	if(instr == INSTR_FUNC_GET)
		regular = FUNC_GETTER;
	else if(instr == INSTR_FUNC_SET)
		regular = FUNC_SETTER;
	var_t* v = func_def(vm, regular,
			(instr == INSTR_FUNC_STC ? true : false));
	if(v != NULL) {
		/* Mark ES6 arrow / generator functions so the runtime can enforce their
		 * special semantics (arrows are not constructible; generators suspend at
		 * `yield` instead of running straight through). */
		if(instr == INSTR_FUNC_ARROW || instr == INSTR_FUNC_GEN) {
			func_t* f = var_get_func(v);
			if(f != NULL) {
				if(instr == INSTR_FUNC_ARROW)
					f->is_arrow = 1;
				else
					f->is_generator = 1;
			}
		}
		/* vm_capture_closure(vm, v);  disabled: def-time capture destabilizes GC */
		/* Block-scope capture (ES6 let per-iteration bindings): a function defined
		 * directly inside a block/loop/try scope captures that block var, so it
		 * still sees the block's bindings after the block scope pops (the compiler
		 * gives each for-let iteration its own block with a copy of the loop var).
		 * Each block var is linked to its lexical parent through the hidden
		 * "@@lex" member so the closure walk can climb past the block into the
		 * enclosing function env (vm_find_in_scopes). Unlike the disabled
		 * vm_capture_closure this never captures a call env at definition time. */
		scope_t* dsc = vm_get_scope(vm);
		if(dsc != NULL && dsc->is_block && !var_empty(dsc->var)) {
			func_t* f = var_get_func(v);
			if(f != NULL && f->closure.var == NULL) {
				scope_t* s = dsc;
				while(s != NULL && !s->is_func) {
					scope_t* p = s->prev;
					if(p != NULL && !var_empty(p->var) && !var_empty(s->var) &&
							var_find_own_member(s->var, "@@lex") == NULL) {
						node_t* ln = var_add(s->var, "@@lex", p->var);
						if(ln != NULL) { ln->invisable = 1; ln->be_unenumerable = 1; }
					}
					s = p;
				}
				f->closure.var = var_ref(dsc->var);
				func_bind_closure_func(f, (s != NULL && s->is_func) ? s->func : NULL);
			}
		}
		vm_push(vm, v);
	}
}

static inline void handle_obj(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* obj;
	if(instr == INSTR_OBJ) {
		obj = var_new_obj(vm, var_get_prototype(vm->builtin_vars.var_Object), NULL, NULL);
	}
	else
		obj = var_new_array(vm);
	scope_t* sc = scope_new(obj);
	vm_push_scope(vm, sc);
}

static inline void handle_obj_end(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* obj = vm_get_scope_var(vm);
	vm_push(vm, obj);
	vm_pop_scope(vm);
}

/* ES6 `{ __proto__: v }`: set the [[Prototype]] of the object literal currently
 * under construction (the scope var) to the popped value. Unlike an ordinary
 * member this does not create an own "__proto__" property. */
static inline void handle_set_proto(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* proto = vm_pop2(vm);
	var_t* obj = vm_get_scope_var(vm);
	if(obj != NULL && proto != NULL && proto->type == V_OBJECT)
		var_set_prototype(obj, proto);
	if(proto != NULL)
		var_unref(proto);
}

/* ---- ES6 iteration protocol -------------------------------------------
 * Array/String get a native iterator object: a plain object holding the source
 * in "@@src" and the cursor in "@@idx" (both hidden members, so GC keeps the
 * source alive via the children map), plus a native next() that returns
 * { value, done }. Map/Set and generators expose [Symbol.iterator] themselves
 * and are driven through the same vm_get_iterator() path. */
static var_t* iter_result(vm_t* vm, var_t* value, bool done) {
	var_t* res = var_new_obj(vm, var_get_prototype(vm->builtin_vars.var_Object), NULL, NULL);
	var_add(res, "value", value != NULL ? value : var_new(vm));
	var_add(res, "done", var_new_bool(vm, done));
	return res;
}

static var_t* native_array_iter_next(vm_t* vm, var_t* env, void* data) {
	(void)data;
	var_t* this_v = get_obj(env, THIS);
	var_t* src = var_find_member_var(this_v, "@@src");
	var_t* idxv = var_find_member_var(this_v, "@@idx");
	uint32_t idx = (idxv != NULL) ? (uint32_t)var_get_int(idxv) : 0;
	uint32_t sz = (src != NULL && src->is_array) ? var_array_size(src) : 0;
	if(idx < sz) {
		if(idxv != NULL) var_set_int(idxv, (int)(idx + 1));
		return iter_result(vm, var_array_get_var(src, (int32_t)idx), false);
	}
	return iter_result(vm, NULL, true);
}

/* number of bytes in the UTF-8 code point starting at byte c */
static inline int utf8_cp_len(unsigned char c) {
	if(c < 0x80) return 1;
	if((c >> 5) == 0x6) return 2;
	if((c >> 4) == 0xE) return 3;
	if((c >> 3) == 0x1E) return 4;
	return 1;
}

static var_t* native_string_iter_next(vm_t* vm, var_t* env, void* data) {
	(void)data;
	var_t* this_v = get_obj(env, THIS);
	var_t* src = var_find_member_var(this_v, "@@src");
	var_t* idxv = var_find_member_var(this_v, "@@idx");
	const char* s = (src != NULL && src->type == V_STRING) ? var_get_str(src) : NULL;
	int len = (s != NULL) ? (int)strlen(s) : 0;
	int idx = (idxv != NULL) ? var_get_int(idxv) : 0;
	if(s != NULL && idx < len) {
		int n = utf8_cp_len((unsigned char)s[idx]);
		if(idx + n > len) n = len - idx;
		if(idxv != NULL) var_set_int(idxv, idx + n);
		return iter_result(vm, var_new_str2(vm, s + idx, (uint32_t)n), false);
	}
	return iter_result(vm, NULL, true);
}

/* Build an array/string iterator over `src`. Returns with refs=0 (caller owns). */
static var_t* new_native_iter(vm_t* vm, var_t* src, bool is_string) {
	var_t* iter = var_new_obj(vm, var_get_prototype(vm->builtin_vars.var_Object), NULL, NULL);
	node_t* sn = var_add(iter, "@@src", src);
	if(sn != NULL) { sn->invisable = 1; sn->be_unenumerable = 1; }
	node_t* in = var_add(iter, "@@idx", var_new_int(vm, 0));
	if(in != NULL) { in->invisable = 1; in->be_unenumerable = 1; }
	vm_reg_native_on(vm, iter, "next()",
		is_string ? native_string_iter_next : native_array_iter_next, NULL);
	return iter;
}

/* A fresh array iterator over `arr`; exported so Map/Set can drive a snapshot
 * array through the same mechanism. Returns with refs=0. */
var_t* vm_new_array_iterator(vm_t* vm, var_t* arr) {
	return new_native_iter(vm, arr, false);
}

/* A fresh string iterator over `str` (code points). Returns with refs=0. */
var_t* vm_new_string_iterator(vm_t* vm, var_t* str) {
	return new_native_iter(vm, str, true);
}

/* GetIterator(iterable): resolve iterable[Symbol.iterator] and call it, else
 * fall back to the built-in array/string iterators. Returns a var the caller
 * owns exactly one reference to (or NULL when not iterable). */
var_t* vm_get_iterator(vm_t* vm, var_t* iterable) {
	if(iterable == NULL)
		return NULL;
	if(iterable->type == V_OBJECT) {
		node_t* m = var_find_member(iterable, SYMKEY_ITERATOR);
		if(m != NULL && m->var != NULL && m->var->is_func) {
			func_call(vm, iterable, m->var, 0); /* pushes the iterator */
			return vm_pop2(vm); /* carries the push ref */
		}
		if(iterable->is_array) {
			var_t* it = new_native_iter(vm, iterable, false);
			var_ref(it);
			return it;
		}
	}
	if(iterable->type == V_STRING) {
		var_t* it = new_native_iter(vm, iterable, true);
		var_ref(it);
		return it;
	}
	return NULL;
}

/* Call iterator.next(); returns the step object with one owned ref (caller
 * unrefs), or NULL when there is no callable next(). */
static var_t* iter_step(vm_t* vm, var_t* iter) {
	node_t* n = var_find_member(iter, "next");
	if(n == NULL || n->var == NULL || !n->var->is_func)
		return NULL;
	func_call(vm, iter, n->var, 0);
	return vm_pop2(vm);
}

/* ---- ES6 generators -----------------------------------------------------
 * A generator body runs in the SAME flat vm_run() loop as its blocks/loops/
 * trys, so a yield always fires in the vm_run frame gen_resume() started:
 * handle_yield sets vm->yielded and that vm_run returns. gen_resume() then
 * detaches the body's scope-stack segment and value-stack segment into the
 * gen_state and re-attaches them on the next next()/throw().
 *
 * GC: the collector only sees the live scope stack / value stack / root, so
 * every var referenced by a detached segment is parked in the generator
 * object's hidden "@@gen_keep" array (the generator itself is reachable
 * through whatever binding holds it). The env and the function var are kept
 * alive the same way via hidden members. */
#define GEN_KEEP "@@gen_keep"

#define GEN_NEXT   0
#define GEN_RETURN 1
#define GEN_THROW  2

typedef struct {
	var_t*   obj;    //the generator object itself (borrowed back pointer)
	var_t*   env;    //the body's call env (borrowed; rooted by "@@gen_env")
	func_t*  func;   //the body's func_t (rooted by "@@gen_func")
	PC       pc;     //resume pc inside the body
	bool     started;
	bool     done;
	scope_t* saved_scopes[VM_SCOPE_STACK_MAX];
	int32_t  saved_scope_num;
	void*    saved_stack[VM_STACK_MAX];
	int32_t  saved_stack_num;
	var_t*   delegate; //in-progress `yield*` iterator (one owned ref, also kept)
} gen_state_t;

static void gen_keep_var(var_t* gen, var_t* v) {
	var_t* keep = var_find_own_member_var(gen, GEN_KEEP);
	if(keep != NULL && !var_empty(v))
		var_array_add(keep, v);
}

static void gen_keep_clear(var_t* gen) {
	var_t* keep = var_find_own_member_var(gen, GEN_KEEP);
	if(keep == NULL)
		return;
	/* Clear only the _ARRAY_ holder so `keep` stays an array, and re-init its
	 * map: hash_map_clean() (via var_remove_all) frees the buckets and leaves
	 * them NULL, which is fine for a dying var but not for one that keeps
	 * receiving var_array_add() on the next suspend. */
	var_t* arr = var_find_own_member_var(keep, "_ARRAY_");
	if(arr != NULL) {
		var_remove_all(arr);
		hash_map_init(&arr->children);
	}
}

/* Detach the body's scope/value stack segments above the bases recorded at
 * resume time, parking every var they reference in the keep array. */
static void gen_suspend(vm_t* vm, gen_state_t* g, int32_t scope_base, int32_t stack_base) {
	int32_t i, n;
	g->pc = vm->pc;

	n = vm->scope_stack_top - scope_base;
	for(i=0; i<n; ++i) {
		scope_t* sc = vm->scope_stack[scope_base + i];
		g->saved_scopes[i] = sc;
		if(sc != NULL)
			gen_keep_var(g->obj, sc->var);
	}
	g->saved_scope_num = n;
	vm->scope_stack_top = scope_base;

	n = vm->stack_top - stack_base;
	for(i=0; i<n; ++i) {
		void* p = vm->stack[stack_base + i];
		g->saved_stack[i] = p;
		if(p != NULL) {
			var_t* v = (*(int8_t*)p == 0) ? (var_t*)p : ((node_t*)p)->var;
			gen_keep_var(g->obj, v);
		}
	}
	g->saved_stack_num = n;
	vm->stack_top = stack_base;
}

/* Re-attach the detached segments on top of the caller's stacks and release
 * the keep anchors. gc is deferred across the release: the caller may still
 * hold the resumption value only in a C local until it is pushed right after. */
static void gen_restore(vm_t* vm, gen_state_t* g) {
	int32_t i;
	vm->gc.gc_defer++;
	for(i=0; i<g->saved_scope_num; ++i)
		vm_push_scope(vm, g->saved_scopes[i]); //re-links prev to the caller's scopes
	g->saved_scope_num = 0;
	for(i=0; i<g->saved_stack_num; ++i) {
		if(vm->stack_top < VM_STACK_MAX)
			vm->stack[vm->stack_top++] = g->saved_stack[i];
	}
	g->saved_stack_num = 0;
	gen_keep_clear(g->obj);
	vm->gc.gc_defer--;
}

/* Drop a suspended body without running it (return(), teardown). */
static void gen_cleanup(vm_t* vm, gen_state_t* g) {
	int32_t i;
	(void)vm;
	for(i=0; i<g->saved_scope_num; ++i)
		scope_free(g->saved_scopes[i]);
	g->saved_scope_num = 0;
	for(i=0; i<g->saved_stack_num; ++i) {
		void* p = g->saved_stack[i];
		if(p != NULL) {
			var_t* v = (*(int8_t*)p == 0) ? (var_t*)p : ((node_t*)p)->var;
			if(!var_empty(v))
				var_unref(v);
		}
	}
	g->saved_stack_num = 0;
	if(g->delegate != NULL) {
		var_unref(g->delegate);
		g->delegate = NULL;
	}
	if(!var_empty(g->obj))
		gen_keep_clear(g->obj);
	g->done = true;
}

/* var_clean() calls this before the children (including "@@gen_keep") are
 * removed, so the detached state is torn down while its anchors still hold. */
static void gen_on_destroy(void* p) {
	var_t* gen = (var_t*)p;
	if(gen == NULL)
		return;
	gen_state_t* g = (gen_state_t*)gen->value;
	if(g != NULL)
		gen_cleanup(gen->vm, g);
}

/* Run (or start) the body until the next yield / return / unwind and build
 * the { value, done } step object. `sent` may be NULL. */
static var_t* gen_resume_body(vm_t* vm, gen_state_t* g, var_t* sent, int mode) {
	int32_t scope_base = vm->scope_stack_top;
	int32_t stack_base = vm->stack_top;
	PC caller_pc = vm->pc;

	if(!g->started) {
		g->started = true;
		if(mode == GEN_THROW) { //nothing ran yet, so nothing inside can catch
			g->done = true;
			vm_throw(vm, "uncaught exception in generator");
			return iter_result(vm, NULL, true);
		}
		scope_t* sc = scope_new(g->env);
		sc->pc = caller_pc; //stale after this resume; every exit path below restores pc itself
		sc->is_func = true;
		sc->func = g->func;
		sc->func_var = (g->func != NULL) ? g->func->owner_var : NULL; //root the generator body's func + lexical chain during the resume
		sc->stack_top = vm->stack_top; //frame baseline for throw truncation
		vm_push_scope(vm, sc);
		vm->pc = g->func->pc;
		//the value sent to the very first next() is discarded per spec: no push
	}
	else {
		gen_restore(vm, g);
		vm->pc = g->pc;
		if(mode == GEN_THROW) {
			/* Simulate INSTR_THROW confined to the body's own frames: unwind to
			 * the nearest try INSIDE the generator, else finish and rethrow. */
			vm_push(vm, (sent != NULL) ? sent : var_new(vm));
			bool caught = false;
			while(vm->scope_stack_top > scope_base) {
				scope_t* sc = vm_get_scope(vm);
				if(sc != NULL && sc->is_try) {
					sc->is_try = false; //consume: a re-throw inside this catch must not loop back to the same handler
					vm->pc = sc->pc;
					caught = true;
					break;
				}
				vm_pop_scope(vm);
			}
			if(!caught) {
				vm_pop(vm); //drop the exception value
				while(vm->stack_top > stack_base)
					vm_pop(vm);
				g->done = true;
				vm->pc = caller_pc;
				vm_throw(vm, "uncaught exception in generator");
				return iter_result(vm, NULL, true);
			}
		}
		else {
			vm_push(vm, (sent != NULL) ? sent : var_new(vm)); //the yield expression's value
		}
	}

	vm->gen_depth++;
	bool returned = vm_run(vm);
	vm->gen_depth--;

	if(vm->yielded) { //suspended at a yield inside this body
		vm->yielded = false;
		vm->gc.gc_defer++; //yield_value lives only in C locals until parked below
		gen_suspend(vm, g, scope_base, stack_base);
		if(vm->yield_delegate != NULL) { //an in-progress `yield*`
			g->delegate = vm->yield_delegate; //transfer the owned ref
			vm->yield_delegate = NULL;
			gen_keep_var(g->obj, g->delegate);
		}
		vm->pc = caller_pc;
		var_t* yv = vm->yield_value; //carries the ref it had on the value stack
		vm->yield_value = NULL;
		var_t* res = iter_result(vm, yv, false);
		if(yv != NULL)
			var_unref(yv);
		vm->gc.gc_defer--;
		return res;
	}

	/* Ran to completion (handle_return already popped the body's scopes) or was
	 * unwound/terminated: collect the return value and settle the stacks. */
	g->done = true;
	while(vm->scope_stack_top > scope_base) //return value (if any) is still stack-rooted here
		vm_pop_scope(vm);
	vm->gc.gc_defer++;
	var_t* rv = NULL;
	if(returned)
		rv = vm_pop2(vm); //carries the push ref from handle_return
	while(vm->stack_top > stack_base)
		vm_pop(vm);
	vm->pc = caller_pc;
	var_t* res = iter_result(vm, rv, true);
	if(rv != NULL)
		var_unref(rv);
	vm->gc.gc_defer--;
	return res;
}

static var_t* gen_resume(vm_t* vm, gen_state_t* g, var_t* sent, int mode) {
	if(g->done)
		return iter_result(vm, (mode == GEN_RETURN) ? sent : NULL, true);

	if(mode == GEN_RETURN) {
		gen_cleanup(vm, g);
		return iter_result(vm, sent, true);
	}

	/* An in-progress `yield*`: forward to the delegate first. */
	if(g->delegate != NULL) {
		const char* mname = (mode == GEN_THROW) ? "throw" : "next";
		node_t* n = var_find_member(g->delegate, mname);
		if(n != NULL && n->var != NULL && n->var->is_func) {
			int argn = 0;
			if(sent != NULL) {
				vm_push(vm, sent);
				argn = 1;
			}
			func_call(vm, g->delegate, n->var, argn);
			var_t* step = vm_pop2(vm); //carries the push ref
			var_t* donev = (step != NULL) ? var_find_member_var(step, "done") : NULL;
			if(step != NULL && (donev == NULL || !var_truthy(donev))) {
				step->refs--; //hand the step back with the plain native return refs
				return step;
			}
			/* Delegate exhausted: its return value is the yield* result; resume
			 * the body with it. Park it in the keep array so the gc triggered by
			 * the teardown below cannot sweep it while only C locals hold it
			 * (gen_restore drops the anchor right before it gets pushed). */
			var_t* dres = (step != NULL) ? var_find_member_var(step, "value") : NULL;
			if(dres != NULL) {
				var_ref(dres);
				gen_keep_var(g->obj, dres);
			}
			if(step != NULL)
				var_unref(step);
			var_unref(g->delegate);
			g->delegate = NULL;
			var_t* res = gen_resume_body(vm, g, dres, GEN_NEXT);
			if(dres != NULL)
				var_unref(dres);
			return res;
		}
		var_unref(g->delegate); //not forwardable: drop it and fall through
		g->delegate = NULL;
	}

	return gen_resume_body(vm, g, sent, mode);
}

static var_t* native_gen_next(vm_t* vm, var_t* env, void* data) {
	gen_state_t* g = (gen_state_t*)data;
	return gen_resume(vm, g, var_find_member_var(env, "v"), GEN_NEXT);
}

static var_t* native_gen_return(vm_t* vm, var_t* env, void* data) {
	gen_state_t* g = (gen_state_t*)data;
	return gen_resume(vm, g, var_find_member_var(env, "v"), GEN_RETURN);
}

static var_t* native_gen_throw(vm_t* vm, var_t* env, void* data) {
	gen_state_t* g = (gen_state_t*)data;
	return gen_resume(vm, g, var_find_member_var(env, "e"), GEN_THROW);
}

/* generator[Symbol.iterator]() returns the generator itself. */
static var_t* native_gen_self(vm_t* vm, var_t* env, void* data) {
	(void)vm; (void)data;
	return get_obj(env, THIS);
}

static var_t* gen_create(vm_t* vm, var_t* func_var, var_t* env) {
	gen_state_t* g = (gen_state_t*)mario_malloc(sizeof(gen_state_t));
	memset(g, 0, sizeof(gen_state_t));

	var_t* gen = var_new_obj(vm, var_get_prototype(vm->builtin_vars.var_Object), g, NULL);
	gen->on_destroy = gen_on_destroy;
	g->obj = gen;
	g->env = env;
	g->func = var_get_func(func_var);
	g->pc = (g->func != NULL) ? g->func->pc : 0;

	node_t* n;
	n = var_add(gen, "@@gen_env", env); //keeps the suspended env alive
	if(n != NULL) { n->invisable = 1; n->be_unenumerable = 1; }
	n = var_add(gen, "@@gen_func", func_var); //keeps the func_t (and its closure) alive
	if(n != NULL) { n->invisable = 1; n->be_unenumerable = 1; }
	n = var_add(gen, GEN_KEEP, var_new_array(vm)); //gc anchor for the detached state
	if(n != NULL) { n->invisable = 1; n->be_unenumerable = 1; }

	vm_reg_native_on(vm, gen, "next(v)", native_gen_next, g);
	vm_reg_native_on(vm, gen, "return(v)", native_gen_return, g);
	vm_reg_native_on(vm, gen, "throw(e)", native_gen_throw, g);
	vm_reg_native_on(vm, gen, SYMKEY_ITERATOR"()", native_gen_self, g);
	return gen;
}

/* INSTR_YIELD / INSTR_YIELD_STAR: hand the value to the driving gen_resume()
 * and make the current vm_run() return (checked right after dispatch). */
static inline void handle_yield(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	(void)ins; (void)offset;
	if(vm->gen_depth == 0) //yield outside a running generator: leave the value as-is
		return;

	if(instr == INSTR_YIELD) {
		var_t* v = vm_pop2(vm); //carries the stack ref
		if(v == NULL)
			v = var_ref(var_new(vm));
		vm->yield_value = v;
		vm->yielded = true;
		return;
	}

	/* yield*: resolve the iterator and take its first step right now; later
	 * steps are forwarded to it by gen_resume() (vm->yield_delegate). */
	var_t* iterable = vm_pop2(vm);
	vm->gc.gc_defer++; //iter/step live only in C locals across the call below
	var_t* iter = (iterable != NULL) ? vm_get_iterator(vm, iterable) : NULL;
	if(iterable != NULL)
		var_unref(iterable);
	var_t* step = (iter != NULL) ? iter_step(vm, iter) : NULL;
	if(step == NULL) {
		if(iter != NULL)
			var_unref(iter);
		vm->gc.gc_defer--;
		vm_push(vm, var_new(vm));
		vm_throw(vm, "yield* target is not iterable");
		return;
	}
	var_t* donev = var_find_member_var(step, "done");
	var_t* val = var_find_member_var(step, "value");
	if(donev != NULL && var_truthy(donev)) {
		//already exhausted: the yield* expression evaluates to its return value
		vm_push(vm, (val != NULL) ? val : var_new(vm));
		var_unref(step);
		var_unref(iter);
		vm->gc.gc_defer--;
		return;
	}
	vm->yield_value = var_ref((val != NULL) ? val : var_new(vm));
	var_unref(step);
	vm->yield_delegate = iter; //transfer the owned ref to gen_resume()
	vm->yielded = true;
	vm->gc.gc_defer--;
}

static inline void handle_get_iter(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	(void)ins; (void)instr; (void)offset;
	var_t* iterable = vm_pop2(vm);
	var_t* iter = vm_get_iterator(vm, iterable);
	if(iter == NULL) {
		vm_push(vm, var_new(vm));
		if(iterable != NULL) var_unref(iterable);
		vm_throw(vm, "object is not iterable");
		return;
	}
	vm_push(vm, iter);
	var_unref(iter); /* the stack now owns the reference */
	if(iterable != NULL)
		var_unref(iterable);
}

/* ES6 array literal spread: [...src]. The array under construction is the
 * current scope variable; append every element of the popped source. Uses the
 * iteration protocol for non-arrays so generators and custom iterables work. */
static inline void handle_arr_spread(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* src = vm_pop2(vm);
	var_t* arr = vm_get_scope_var(vm);
	if(src != NULL && arr != NULL) {
		if(src->is_array) {
			uint32_t sz = var_array_size(src);
			uint32_t i;
			for(i=0; i<sz; i++) {
				var_t* e = var_array_get_var(src, (int32_t)i);
				if(e != NULL)
					var_array_add(arr, e);
			}
		}
		else {
			var_t* iter = vm_get_iterator(vm, src);
			if(iter != NULL) {
				/* iter/step are held only by C locals; defer gc across the loop. */
				vm->gc.gc_defer++;
				for(;;) {
					var_t* step = iter_step(vm, iter);
					if(step == NULL)
						break;
					var_t* donev = var_find_member_var(step, "done");
					if(donev != NULL && var_truthy(donev)) {
						var_unref(step);
						break;
					}
					var_t* val = var_find_member_var(step, "value");
					if(val != NULL)
						var_array_add(arr, val);
					var_unref(step);
				}
				vm->gc.gc_defer--;
				var_unref(iter);
			}
		}
	}
	if(src != NULL)
		var_unref(src);
}

typedef struct {
	var_t* target;
} obj_spread_data;

static void obj_spread_cb(const char* key, void* value, void* user_data) {
	(void)key;
	obj_spread_data* d = (obj_spread_data*)user_data;
	node_t* node = (node_t*)value;
	if(node != NULL &&
			node->be_inherited == 0 &&
			node->invisable == 0 &&
			!node->be_unenumerable) {
		var_add(d->target, node->name, node->var);
	}
}

/* ES6 object literal spread: {...src}. Copy src's own enumerable members into
 * the object under construction (the current scope variable). */
static inline void handle_obj_spread(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* src = vm_pop2(vm);
	var_t* target = vm_get_scope_var(vm);
	if(src != NULL && target != NULL) {
		obj_spread_data d;
		d.target = target;
		hash_map_iterate(&src->children, obj_spread_cb, &d);
	}
	if(src != NULL)
		var_unref(src);
}

/* ES6 call with spread arguments: f(...arr). The runtime args array is on top
 * of the stack. For INSTR_CALLO_SPREAD the receiver object sits just beneath. */
static inline void handle_call_spread(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	mstr_t* name = mstr_new("");
	parse_func_name(s, name); // strip any $arity suffix, resolve by base name

	var_t* args = vm_pop2(vm);
	if(args == NULL || !args->is_array) {
		if(args != NULL) var_unref(args);
		args = var_new_array(vm);
		var_ref(args);
	}

	var_t* obj = NULL;
	var_t* func = NULL;
	if(instr == INSTR_CALLO_SPREAD) {
		obj = vm_stack_pick(vm, 1);
		if(obj != NULL)
			func = find_func(vm, obj, name->cstr);
	}
	else if(instr == INSTR_CALLX_SPREAD) {
		/* call-by-value with spread: the callable sits right below the args
		 * array on the value stack. */
		func = vm_stack_pick(vm, 1);
		obj = vm_this_in_scopes(vm);
	}
	else if(instr == INSTR_CALLXO_SPREAD) {
		/* `obj[k](...args)`: below the args array sit the func value then the
		 * receiver (INSTR_ARRAY_AT_M). Pick func then receiver. */
		func = vm_stack_pick(vm, 1);
		obj = vm_stack_pick(vm, 1);
	}
	else {
		var_t* sc_var = vm_get_scope_var(vm);
		obj = vm_this_in_scopes(vm);
		func = find_func(vm, sc_var, name->cstr);
		if(func == NULL && obj != NULL)
			func = find_func(vm, obj, name->cstr);
	}

	if(func != NULL && !func->is_func && !var_is_proxy(func)) {
		var_t* constr = var_find_own_member_var(func, CONSTRUCTOR);
		if(constr == NULL) {
			var_t* protoV = var_get_prototype(func);
			if(protoV != NULL)
				func = var_find_own_member_var(protoV, CONSTRUCTOR);
			else
				func = NULL;
		}
		else {
			func = constr;
		}
	}

	if(func != NULL) {
		var_array_reverse(args); // call_m_func expects last arg at index 0
		var_t* ret = call_m_func(vm, obj, func, args);
		vm->gc.gc_defer++; //args/obj/func are bare C pointers from here to the last unref
		if(ret == NULL)
			ret = var_new(vm);
		vm_push(vm, ret);
		var_unref(ret);
	}
	else {
		vm->gc.gc_defer++; //same guard for the throw-unwind path
		vm_push(vm, var_new(vm));
		vm_throw(vm, "can not find function '%s'!", name->cstr);
	}

	mstr_free(name);
	var_unref(args);
	if(instr == INSTR_CALLO_SPREAD && obj != NULL)
		var_unref(obj);
	if(instr == INSTR_CALLX_SPREAD && func != NULL)
		var_unref(func); // release the ref the value-stack slot held
	if(instr == INSTR_CALLXO_SPREAD) {
		if(func != NULL) var_unref(func);
		if(obj != NULL) var_unref(obj);
	}
	vm->gc.gc_defer--;
}

/* ES6 construct with spread arguments: new C(...arr). Push the array elements
 * individually then reuse the standard construction path. */
static inline void handle_new_spread(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	mstr_t* name = mstr_new("");
	parse_func_name(s, name);

	var_t* args = vm_pop2(vm);
	int arg_num = 0;
	if(args != NULL && args->is_array) {
		vm_push(vm, args); //anchor: keep args gc-reachable while the ctor runs (see call_m_func)
		arg_num = var_array_size(args);
		int i;
		for(i = 0; i < arg_num; i++) {
			node_t* node = var_array_get(args, i);
			if(node == NULL || node->var == NULL)
				vm_push(vm, var_new(vm));
			else
				vm_push(vm, node->var);
		}
	}

	var_t* obj = new_obj(vm, name->cstr, arg_num);
	vm->gc.gc_defer++; //obj/args are bare C pointers until pushed/released
	if(args != NULL && args->is_array)
		vm_pop(vm); //drop the args anchor
	if(obj == NULL) {
		vm_push(vm, var_new(vm));
		vm_terminate(vm);
	}
	else {
		vm_push(vm, obj);
	}
	mstr_free(name);
	if(args != NULL)
		var_unref(args);
	vm->gc.gc_defer--;
}

/* ES6 computed member in an object literal: {[key]: value}. The object under
 * construction is the current scope variable; key then value are on the stack. */
static inline void handle_memberv(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	var_t* k = vm_pop2(vm);
	if(v == NULL)
		v = var_new(vm);
	const char* key = "";
	mstr_t* ks = NULL;
	bool key_is_symbol = false;
	if(k != NULL) {
		if(var_is_symbol(k)) {
			const char* sk = var_symbol_key(k);
			if(sk != NULL) { key = sk; key_is_symbol = true; }
		}
		if(!key_is_symbol) {
			ks = mstr_new("");
			var_to_str(k, ks);
			key = ks->cstr;
		}
	}
	var_t* var = vm_get_scope_var(vm);
	if(var->is_array) {
		var_array_add(var, v);
	}
	else {
		if(v->is_func) {
			func_t* func = (func_t*)v->value;
			func->owner = var;
		}
		node_t* n = var_add(var, key, v);
		if(key_is_symbol && n != NULL)
			n->be_unenumerable = 1; /* symbol keys hidden from Object.keys/for..in */
	}
	var_unref(v);
	if(k != NULL)
		var_unref(k);
	if(ks != NULL)
		mstr_free(ks);
}

/* Decode a subscript key into a TypedArray element index. Returns -1 (-> read
 * yields undefined / write is ignored) for a non-canonical or fractional key. */
static int64_t ta_key_index(var_t* v2) {
	if(v2->type == V_STRING) {
		const char* s = var_get_str(v2);
		char* end = NULL;
		int64_t idx = (s != NULL && *s != 0) ? (int64_t)strtoll(s, &end, 10) : -1;
		if(end == NULL || *end != 0) return -1; /* non-canonical -> undefined */
		return idx;
	}
	if(v2->type == V_FLOAT || v2->type == V_FLOAT64) {
		double d = var_get_float64(v2);
		return (d == floor(d) && !isinf(d)) ? (int64_t)d : -1;
	}
	return var_get_int64(v2);
}

/* Shared post-pop body of the subscript operators: v1 (receiver) and v2 (key)
 * arrive owning their value-stack references and are released here. Pushes the
 * element/member value or, for a persistent receiver, the binding node so a
 * following assignment can write through it. */
static void array_at_push(vm_t* vm, var_t* v1, var_t* v2) {
	/* Proxy indexed read: `p[key]` routes the get trap with the key var (string,
	 * symbol or number). v1/v2 arrive holding the caller's popped stack refs, which
	 * this branch releases exactly like the plain path below. */
	if(var_is_proxy(v1)) {
		var_t* res = proxy_get(vm, v1, v2, v1);   // v2 is borrowed (refs>=1)
		vm_push(vm, (res != NULL) ? res : var_new(vm));
		var_unref(v1);
		var_unref(v2);
		return;
	}
	/* TypedArray indexed read: `ta[i]` (or canonical `ta["i"]`) decodes element i
	 * straight from the shared buffer. The cheap `!is_array && var_is_typedarray`
	 * guard is one hash miss for every plain object/array, so the hot path is a
	 * genuine no-op for them. Symbol keys (e.g. @@iterator) fall through to the
	 * normal member lookup below. OOB / non-canonical index -> undefined. */
	if(!v1->is_array && var_is_typedarray(v1) && !var_is_symbol(v2)) {
		var_t* el = var_typedarray_get_at(vm, v1, ta_key_index(v2));
		vm_push(vm, (el != NULL) ? el : var_new(vm));
		var_unref(v1);
		var_unref(v2);
		return;
	}
	node_t* n = NULL;
	if(var_is_symbol(v2)) {
		/* ES6 symbol key: obj[sym] resolves through the symbol's unique key. */
		const char* sk = var_symbol_key(v2);
		if(sk != NULL)
			n = var_find_member_create(v1, sk);
	}
	else if(v2->type == V_STRING) {
		const char* s = var_get_str(v2);
		n = var_find_member_create(v1, s);
	}
	else if(v1->type == V_STRING) {
		/* ES6: indexing a string yields the character at that position,
		 * e.g. "abc"[1] == "b"; out-of-range gives undefined (as in JS).
		 * This also makes `for...of` work over strings. */
		int at = var_get_int(v2);
		const char* s = var_get_str(v1);
		int len = (s != NULL) ? (int)strlen(s) : 0;
		if(at >= 0 && at < len) {
			char ch[2] = { s[at], 0 };
			vm_push(vm, var_new_str(vm, ch));
		}
		else {
			vm_push(vm, var_new(vm));
		}
		var_unref(v1);
		var_unref(v2);
		return;
	}
	else {
		int at = var_get_int(v2);
		n = var_array_get(v1, at);
	}
	if(n != NULL) {
		/* If v1 is transient (its only reference is the one released just
		 * below), the node lives inside v1 and would dangle once v1 is freed
		 * (e.g. `getArr()[1]`). Push the value instead (rvalue). For a
		 * persistent v1 (named variable) keep node semantics so that
		 * `arr[i] = x` can still write through the node. */
		if(v1->refs <= 1)
			vm_push(vm, n->var);
		else
			vm_push_node(vm, n);
	}
	else
		vm_push(vm, var_new(vm));
	var_unref(v1);
	var_unref(v2);
}

static inline void handle_array_at(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		if(v1 != NULL) var_unref(v1);
		if(v2 != NULL) var_unref(v2);
		return;
	}
	array_at_push(vm, v1, v2);
}

/* Subscript as an assignment target (`ta[i] = ..`, `ta[i] += ..`, `ta[i]++`).
 * For a TypedArray receiver push a SYNTHETIC @@taslot node: a magic=1 node whose
 * ->var is the decoded current element (so a compound op / ++ reads it) carrying
 * a hidden @@ta member (the ref'd TypedArray) and whose ncache_instr is the
 * index. handle_asign / math_result / vm_step_op recognise the sentinel, encode
 * the new value straight into the shared buffer, and free the node. For every
 * other receiver this is exactly handle_array_at (normal arrays already yield a
 * writable binding node), so the compiler retarget is type-agnostic. */
static inline void handle_array_at_w(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		if(v1 != NULL) var_unref(v1);
		if(v2 != NULL) var_unref(v2);
		return;
	}
	/* Proxy subscript write: `p[key] = v` / `p[key] += v` / `p[key]++` push a
	 * @@proxyslot sentinel carrying the proxy + key; handle_asign / math_result /
	 * vm_step_op detect it and drive the set trap. The sentinel adopts one ref each
	 * on v1/v2, so release the popped stack refs here (mirrors the TA branch). */
	if(var_is_proxy(v1)) {
		proxy_push_slot(vm, v1, v2);
		var_unref(v1);
		var_unref(v2);
		return;
	}
	if(!v1->is_array && var_is_typedarray(v1) && !var_is_symbol(v2)) {
		int64_t idx = ta_key_index(v2);
		vm->gc.gc_defer++;   /* sn/cur are unrooted until vm_push_node below */
		node_t* sn = (node_t*)mario_malloc(sizeof(node_t));
		memset(sn, 0, sizeof(node_t));
		sn->magic = 1;
		sn->name = (char*)mario_malloc(strlen(TA_SLOT)+1);
		memcpy(sn->name, TA_SLOT, strlen(TA_SLOT)+1);
		var_t* cur = var_typedarray_get_at(vm, v1, idx);
		if(cur == NULL) cur = var_new(vm);
		sn->var = var_ref(cur);                    /* node's own reference */
		node_t* tn = var_add(cur, TA_SLOT_TA, v1); /* hidden back-ref (var_add refs v1) */
		tn->invisable = 1; tn->be_unenumerable = 1;
		sn->ncache_instr = (uint32_t)(int32_t)idx;
		vm_push_node(vm, sn);                      /* adds the stack reference to cur */
		vm->gc.gc_defer--;
		var_unref(v1);   /* balance pop2; the @@ta member keeps the TA alive */
		var_unref(v2);
		return;
	}
	array_at_push(vm, v1, v2);
}


/* `obj[key]` used as a call target `obj[key](...)`: resolve the member but keep
 * the receiver on the stack (beneath the member value) so INSTR_CALLXO can bind
 * `this`. Mirrors handle_array_at's lookup; the string-index case is irrelevant
 * here (indexing a string then calling it is not a supported method call). */
static inline void handle_array_at_m(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	(void)ins; (void)instr; (void)offset;
	var_t* v2 = vm_pop2(vm); /* key */
	var_t* v1 = vm_pop2(vm); /* receiver */
	node_t* n = NULL;
	if(v1 != NULL && v2 != NULL) {
		if(var_is_symbol(v2)) {
			const char* sk = var_symbol_key(v2);
			if(sk != NULL)
				n = var_find_member_create(v1, sk);
		}
		else if(v2->type == V_STRING)
			n = var_find_member_create(v1, var_get_str(v2));
		else
			n = var_array_get(v1, var_get_int(v2));
	}
	if(v1 != NULL)
		vm_push(vm, v1); /* receiver stays for `this` */
	else
		vm_push(vm, var_new(vm));
	if(n != NULL && n->var != NULL)
		vm_push(vm, n->var);
	else
		vm_push(vm, var_new(vm));
	if(v1 != NULL) var_unref(v1);
	if(v2 != NULL) var_unref(v2);
}

static inline void handle_class(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	const char* s = bc_getstr(&vm->bc, offset);
	/* An anonymous class expression (`const P = class {}`) carries an empty name.
	 * Give each one a unique internal binding so successive anonymous classes do
	 * not reuse (and clobber) the same scope entry. */
	char anon[32];
	if(s == NULL || s[0] == 0) {
		static uint32_t anon_id = 0;
		snprintf(anon, sizeof(anon), "@class$%u", anon_id++);
		s = anon;
	}
	var_t* cls_var = vm_new_class(vm, s);
	ins = code[vm->pc];
	instr = OP(ins);
	if(instr == INSTR_EXTENDS) {
		vm->pc++;
		offset = OFF(ins);
		const char* sup = bc_getstr(&vm->bc, offset);
		do_extends(vm, cls_var, sup);
	}

	var_t* protoV = var_get_prototype(cls_var);
	scope_t* sc = scope_new(protoV);
	sc->class_var = cls_var; // CLASS_END pushes the constructor so class expressions evaluate to it
	vm_push_scope(vm, sc);
}

static inline void handle_class_end(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	scope_t* sc = vm_get_scope(vm);
	/* Push the constructor (class) value, not the prototype, so a class
	 * expression evaluates to something `new`-able. For a class declaration the
	 * pushed value is simply discarded by the enclosing statement. */
	var_t* cls = (sc != NULL) ? sc->class_var : NULL;
	if(cls == NULL)
		cls = vm_get_scope_var(vm);
	vm_push(vm, cls);
	vm_pop_scope(vm);
}

static inline void handle_instof(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	bool res = false;
	if(v1 != NULL && v2 != NULL) {
		res = var_instanceof(v1, v2);
	}
	vm->gc.gc_defer++; //v1/v2 are bare C pointers: the first unref may gc-sweep the second (see vm_step_op)
	if(v2 != NULL) var_unref(v2);
	if(v1 != NULL) var_unref(v1);
	vm_push(vm, var_new_bool(vm, res));
	vm->gc.gc_defer--;
}

static inline void handle_typeof(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* var = vm_pop2(vm);
	if(var == NULL) {
		vm_push(vm, var_new_str(vm, "undefined"));
		return;
	}
	var_t* v = var_new_str(vm, get_typeof(var));
	var_unref(var);
	vm_push(vm, v);
}

/* `delete o.x` (INSTR_DELETE $name): the object is on the stack; remove its own
 * member $name and push the boolean result. The compiler retargets the operand's
 * final INSTR_GET here (see unary()), so it keeps GET's pop-1/push-1 arity. */
static inline void handle_delete(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* obj = vm_pop2(vm);
	vm->gc.gc_defer++; //obj is a bare C pointer across the member teardown (see handle_instof)
	bool res;
	if(var_is_proxy(obj)) {
		var_t* keyv = var_new_str(vm, s);
		var_ref(keyv);                       // own it across proxy_delete's borrow
		res = proxy_delete(vm, obj, keyv);   // `delete p.x`: deleteProperty trap
		var_unref(keyv);
	}
	else {
		res = var_delete_own_member(obj, s);
	}
	if(obj != NULL) var_unref(obj);
	vm_push(vm, var_new_bool(vm, res));
	vm->gc.gc_defer--;
}

/* `delete o[k]` (INSTR_DELETE_AT): the key is on top of the object. Retargeted
 * from the operand's final INSTR_ARRAY_AT; keeps its pop-2/push-1 arity. */
static inline void handle_delete_at(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* key = vm_pop2(vm);
	var_t* obj = vm_pop2(vm);
	vm->gc.gc_defer++;
	bool res = true;
	if(obj != NULL && key != NULL) {
		if(var_is_proxy(obj)) {
			res = proxy_delete(vm, obj, key);   // `delete p[k]`: deleteProperty trap (key borrowed)
		}
		else if(var_is_symbol(key)) {
			res = var_delete_own_member(obj, var_symbol_key(key));
		}
		else if(key->type == V_STRING) {
			res = var_delete_own_member(obj, var_get_str(key));
		}
		else {
			/* Numeric/other key: mirrors handle_array_at and is treated as an
			 * index. Array elements live in the nested _ARRAY_ store; a plain
			 * object keeps the decimal-keyed member directly on itself. */
			var_t* cont = obj->is_array ? var_find_own_member_var(obj, "_ARRAY_") : obj;
			if(cont != NULL) {
				char k[32];
				snprintf(k, sizeof(k), "%d", var_get_int(key));
				res = var_delete_own_member(cont, k);
			}
		}
	}
	if(key != NULL) var_unref(key);
	if(obj != NULL) var_unref(obj);
	vm_push(vm, var_new_bool(vm, res));
	vm->gc.gc_defer--;
}

/* `delete x` (INSTR_DELETE_VAR $name): a bare identifier. Only a global (an own
 * property of the root object) is deletable; a resolved local/lexical binding is
 * non-configurable and yields false; an unresolvable name yields true (JS
 * non-strict). Retargeted from the operand's INSTR_LOAD; keeps its push-1 arity. */
static inline void handle_delete_var(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	bool res;
	node_t* node = vm_find_in_scopes(vm, s);
	if(node == NULL) {
		res = true;                        //unresolvable reference: delete yields true
	}
	else if(var_find_own_member(vm->root, s) != NULL) {
		var_delete_own_member(vm->root, s);
		res = true;                        //deleted a global own property
	}
	else {
		res = false;                       //local / lexical binding: not deletable
	}
	vm_push(vm, var_new_bool(vm, res));
}

/* `key in obj` (INSTR_IN): the object (RHS) sits on top of the key (LHS). Push
 * whether the key is present on obj's own or prototype chain. */
static inline void handle_in(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* obj = vm_pop2(vm);
	var_t* key = vm_pop2(vm);
	vm->gc.gc_defer++;
	bool res = false;
	if(obj != NULL && key != NULL) {
		if(var_is_proxy(obj)) {
			res = proxy_has(vm, obj, key);   // `key in p`: has trap (key borrowed, refs>=1)
		}
		else if(var_is_symbol(key)) {
			const char* sk = var_symbol_key(key);
			if(sk != NULL) res = var_has_member(obj, sk);
		}
		else if(key->type == V_STRING) {
			res = var_has_member(obj, var_get_str(key));
		}
		else {
			char k[32];
			snprintf(k, sizeof(k), "%d", var_get_int(key));
			res = var_has_member(obj, k);
		}
	}
	if(key != NULL) var_unref(key);
	if(obj != NULL) var_unref(obj);
	vm_push(vm, var_new_bool(vm, res));
	vm->gc.gc_defer--;
}

/* ===================== Proxy internals (Phase 5) =====================
 * A proxy is an ordinary V_OBJECT marked @@exotic="proxy" with hidden @@ptarget /
 * @@phandler / @@prevoked members (installed by native_Proxy.c). Every property
 * path in the VM - do_get, array_at_push, the @@proxyslot write sentinel,
 * handle_in, handle_delete/_at, func_call and new_obj - checks var_is_proxy() and
 * routes here; for a plain object each check is a single hash miss, so the
 * intercept is a genuine no-op.
 *
 * Each proxy_<op> invokes handler.<trap>(target, ...) when that trap is a function
 * and otherwise performs the DEFAULT operation on the target (forwarding through
 * mario_<op>_var, so a proxy-over-a-proxy chains correctly). A revoked proxy throws
 * a TypeError from every entry point. Trap results are returned with call_m_func's
 * extra reference released (refs == baseline), exactly like a native return value or
 * new_obj: the caller roots it (vm_push) or hands it to func_call. Invariants this
 * VM cannot model (it does not track configurability/writability) are not enforced;
 * the callable-target, object-or-null-prototype and preventExtensions invariants
 * are. proxy_call_trap borrows its argv (each element must carry refs>=1). */

/* Normalise a property-key var (string / symbol / number / bool) to a C string.
 * Numeric and boolean keys render into `numbuf` (which the caller keeps alive);
 * string and symbol keys return their own storage. */
static const char* proxy_key_cstr(var_t* key, char* numbuf, uint32_t sz) {
	if(key == NULL)
		return "";
	if(key->type == V_STRING)
		return var_get_str(key);
	if(var_is_symbol(key)) {
		const char* s = var_symbol_key(key);
		return (s != NULL) ? s : "";
	}
	if(numbuf == NULL || sz == 0)
		return "";
	switch(key->type) {
		case V_INT:   snprintf(numbuf, sz, "%d", key->value ? *(int*)key->value : 0); break;
		case V_INT64: snprintf(numbuf, sz, "%lld", key->value ? (long long)*(int64_t*)key->value : 0LL); break;
		case V_BOOL:  snprintf(numbuf, sz, "%s", var_get_bool(key) ? "true" : "false"); break;
		case V_NULL:  snprintf(numbuf, sz, "null"); break;
		case V_UNDEF: numbuf[0] = 0; break;
		default:      snprintf(numbuf, sz, "%d", var_get_int(key)); break;
	}
	return numbuf;
}

var_t* var_proxy_target(var_t* p)  { return var_find_own_member_var(p, PROXY_TARGET); }
var_t* var_proxy_handler(var_t* p) { return var_find_own_member_var(p, PROXY_HANDLER); }

bool var_proxy_is_revoked(var_t* p) {
	var_t* r = var_find_own_member_var(p, PROXY_REVOKED);
	return (r != NULL) ? var_get_bool(r) : false;
}

/* Callable = a function, or a (non-revoked) proxy whose target is callable, so the
 * call/new paths route an apply/construct trap instead of "not a function". */
bool var_is_callable(var_t* v) {
	if(v == NULL)
		return false;
	if(v->is_func)
		return true;
	if(var_is_proxy(v) && !var_proxy_is_revoked(v))
		return var_is_callable(var_proxy_target(v));
	return false;
}

static bool proxy_throw_revoked(vm_t* vm) {
	vm_throw_type_native(vm, "TypeError", "Cannot perform operation on a revoked proxy");
	return true;
}

/* handler.<trap> when it is a function, else NULL (caller uses the default op). */
static var_t* proxy_trap_func(var_t* p, const char* trap) {
	var_t* h = var_proxy_handler(p);
	if(h == NULL)
		return NULL;
	var_t* fn = var_find_member_var(h, trap);
	return (fn != NULL && fn->is_func) ? fn : NULL;
}

/* Invoke a trap: fn(target, argv[0..argc-1]) with this=handler. argv elements are
 * BORROWED and must carry refs>=1 (the args array adopts one ref per element and
 * releases it, so refcounts are unchanged and the elements survive). Returns an
 * OWNED result (refs>=1) or NULL. */
static var_t* proxy_call_trap(vm_t* vm, var_t* p, var_t* fn, var_t** argv, int argc) {
	var_t* target = var_proxy_target(p);
	var_t* handler = var_proxy_handler(p);
	var_t* args = var_new_array(vm);
	var_array_add(args, (target != NULL) ? target : var_new(vm));
	for(int i = 0; i < argc; i++)
		var_array_add(args, (argv[i] != NULL) ? argv[i] : var_new(vm));
	var_array_reverse(args);
	var_t* res = call_m_func(vm, handler, fn, args);
	var_unref(args);
	return res;
}

/* Call `func` with this=thisArg and the natural-order args array. OWNED result. */
static var_t* call_with_args(vm_t* vm, var_t* thisArg, var_t* func, var_t* argsNatural) {
	var_t* args = var_new_array(vm);
	int n = (argsNatural != NULL) ? (int)var_array_size(argsNatural) : 0;
	for(int i = 0; i < n; i++) {
		node_t* nd = var_array_get(argsNatural, i);
		var_array_add(args, (nd != NULL && nd->var != NULL) ? nd->var : var_new(vm));
	}
	var_array_reverse(args);
	var_t* res = call_m_func(vm, thisArg, func, args);
	var_unref(args);
	return res;
}

/* Collect the OWN property keys of a plain object/array (no prototype chain).
 * Array elements live in the nested _ARRAY_ store, so indices come from there. */
typedef struct { vm_t* vm; var_t* keys; bool enum_only; hash_map_t* seen; } proxy_keys_ctx;
static void proxy_keys_cb(const char* key, void* value, void* ud) {
	(void)key;
	proxy_keys_ctx* d = (proxy_keys_ctx*)ud;
	node_t* node = (node_t*)value;
	if(node == NULL || node->be_inherited || node->invisable)
		return;
	if(d->enum_only && node->be_unenumerable)
		return;
	if(hash_map_get(d->seen, node->name) != NULL)
		return;
	hash_map_add(d->seen, node->name, (void*)"");
	var_array_add(d->keys, var_new_str(d->vm, node->name));
}
static var_t* own_keys_default(vm_t* vm, var_t* obj, bool enum_only) {
	var_t* keys = var_new_array(vm);
	if(obj == NULL)
		return keys;
	vm->gc.gc_defer++;   // keys is unrooted here
	hash_map_t* seen = hash_map_new();
	proxy_keys_ctx d; d.vm = vm; d.keys = keys; d.enum_only = enum_only; d.seen = seen;
	var_t* cont = obj->is_array ? var_find_own_member_var(obj, "_ARRAY_") : obj;
	if(cont != NULL)
		hash_map_iterate(&cont->children, proxy_keys_cb, &d);
	hash_map_free(seen, mario_free, NULL);
	vm->gc.gc_defer--;
	return keys;   // refs=0
}

static bool mario_is_extensible(var_t* obj) {
	if(obj == NULL)
		return false;
	var_t* nx = var_find_own_member_var(obj, OBJ_NO_EXT);
	return !(nx != NULL && var_get_bool(nx));
}

/* Default [[DefineOwnProperty]] on a plain target, mirroring native_Object. */
static bool mario_define_default(vm_t* vm, var_t* obj, var_t* key, var_t* desc) {
	(void)vm;
	if(obj == NULL)
		return false;
	char numbuf[32];
	const char* ks = proxy_key_cstr(key, numbuf, sizeof(numbuf));
	var_t* val = (desc != NULL) ? var_find_own_member_var(desc, "value") : NULL;
	node_t* node = var_add(obj, ks, val);
	if(node == NULL)
		return false;
	if(desc != NULL) {
		var_t* w = var_find_own_member_var(desc, "writable");
		if(w != NULL) node->be_const = !var_get_bool(w);
		var_t* e = var_find_own_member_var(desc, "enumerable");
		if(e != NULL) node->be_unenumerable = !var_get_bool(e);
		var_t* c = var_find_own_member_var(desc, "configurable");
		if(c != NULL) node->be_const = !var_get_bool(c);
	}
	return true;
}

/* Default [[GetOwnProperty]] on a plain target: a fresh descriptor object, or
 * undefined when the own property is absent. */
static var_t* mario_gopd_default(vm_t* vm, var_t* obj, var_t* key) {
	if(obj == NULL)
		return var_new(vm);
	char numbuf[32];
	const char* ks = proxy_key_cstr(key, numbuf, sizeof(numbuf));
	node_t* node = var_find_own_member(obj, ks);
	if(node == NULL)
		return var_new(vm);
	var_t* d = var_new_obj_no_proto(vm, NULL, NULL);
	var_add(d, "value", node->var);
	var_add(d, "writable", var_new_bool(vm, !node->be_const));
	var_add(d, "enumerable", var_new_bool(vm, !node->be_unenumerable));
	var_add(d, "configurable", var_new_bool(vm, !node->be_const));
	return d;   // refs=0
}

/* `new ctor(...args)` from a constructor VAR (a proxy forwards to its construct
 * trap). Returns a refs==0 object, matching new_obj()'s contract. */
static var_t* construct_from_var(vm_t* vm, var_t* ctor, var_t* argsNatural, var_t* newTarget) {
	if(ctor == NULL || !var_is_callable(ctor)) {
		vm_throw_type_native(vm, "TypeError", "target is not a constructor");
		return var_new(vm);
	}
	if(var_is_proxy(ctor))
		return proxy_construct(vm, ctor, argsNatural, (newTarget != NULL) ? newTarget : ctor);
	var_t* proto = var_get_prototype(ctor);
	var_t* obj = var_new_obj(vm, proto, NULL, NULL);
	var_ref(obj);   // protect across func_call (mirrors new_obj)
	int arg_num = (argsNatural != NULL) ? (int)var_array_size(argsNatural) : 0;
	vm->gc.gc_defer++;
	for(int i = 0; i < arg_num; i++) {
		node_t* nd = var_array_get(argsNatural, i);
		vm_push(vm, (nd != NULL && nd->var != NULL) ? nd->var : var_new(vm));
	}
	var_t* old_nt = vm->new_target;
	vm->new_target = (newTarget != NULL) ? newTarget : ctor;
	func_call(vm, obj, ctor, arg_num);
	vm->new_target = old_nt;
	var_t* ret = vm_pop2(vm);
	if(ret != NULL && ret != obj && ret->type == V_OBJECT) {
		obj->refs--;
		obj = ret;
		obj->refs--;
	}
	else {
		if(ret != NULL) ret->refs--;
		obj->refs--;
	}
	vm->gc.gc_defer--;
	return obj;   // refs=0
}

/* ---- proxy-aware primitives (shared by the VM intercept and Reflect.*) ---- */

var_t* mario_get_var(vm_t* vm, var_t* obj, var_t* key, var_t* receiver) {
	if(obj == NULL)
		return var_new(vm);
	if(var_is_proxy(obj))
		return proxy_get(vm, obj, key, receiver);
	char numbuf[32];
	const char* ks = proxy_key_cstr(key, numbuf, sizeof(numbuf));
	var_t* res = NULL;
	if(key != NULL && key->type == V_STRING) {
		do_get(vm, obj, ks, false);          // accessor-aware; pushes (does not consume obj)
		res = vm_pop2(vm);
	}
	else {
		var_ref(obj);
		var_t* k = (key != NULL) ? key : var_new(vm);
		var_ref(k);
		array_at_push(vm, obj, k);           // consumes both refs; pushes
		res = vm_pop2(vm);
	}
	if(res != NULL && res->refs > 0) res->refs--;   // release the push ref -> baseline
	return (res != NULL) ? res : var_new(vm);
}

bool mario_set_var(vm_t* vm, var_t* obj, var_t* key, var_t* value, var_t* receiver) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_set(vm, obj, key, value, receiver);
	char numbuf[32];
	const char* ks = proxy_key_cstr(key, numbuf, sizeof(numbuf));
	var_t* val = (value != NULL) ? value : var_new(vm);
	node_t* n = var_find_member(obj, ks);
	if(n != NULL && var_is_accessor(n->var)) {
		var_t* setter = var_accessor_setter(n->var);
		if(setter == NULL)
			return false;                     // read-only accessor: ignore (non-strict)
		var_ref(setter);
		var_t* args = var_new_array(vm);
		var_array_add(args, val);
		var_array_reverse(args);
		var_t* r = call_m_func(vm, (receiver != NULL) ? receiver : obj, setter, args);
		var_unref(args);
		if(r != NULL) var_unref(r);
		var_unref(setter);
		return true;
	}
	if(obj->is_array && key != NULL && key->type != V_STRING && !var_is_symbol(key)) {
		var_array_set(obj, var_get_int(key), val);
		return true;
	}
	node_t* on = var_find_own_member(obj, ks);
	if(on == NULL)
		on = var_add(obj, ks, NULL);
	if(on == NULL)
		return false;
	node_replace(on, val);
	return true;
}

bool mario_has_var(vm_t* vm, var_t* obj, var_t* key) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_has(vm, obj, key);
	char numbuf[32];
	return var_has_member(obj, proxy_key_cstr(key, numbuf, sizeof(numbuf)));
}

bool mario_delete_var(vm_t* vm, var_t* obj, var_t* key) {
	(void)vm;
	if(obj == NULL)
		return true;
	if(var_is_proxy(obj))
		return proxy_delete(vm, obj, key);
	char numbuf[32];
	return var_delete_own_member(obj, proxy_key_cstr(key, numbuf, sizeof(numbuf)));
}

var_t* mario_own_keys_var(vm_t* vm, var_t* obj, bool strings_only, bool enum_only) {
	(void)strings_only;
	if(obj == NULL)
		return var_new_array(vm);
	if(var_is_proxy(obj))
		return proxy_own_keys(vm, obj, strings_only, enum_only);
	return own_keys_default(vm, obj, enum_only);
}

/* Proxy-aware primitives for the remaining internal methods, shared by Reflect.*
 * and the Object.* statics: a proxy routes its trap, any other object the default
 * operation on itself. Return contract matches the mario_*_var read/write family -
 * var-returning ops yield a baseline (refs==0) value or a borrowed persistent
 * prototype, bool ops yield the operation result. The proxy_* entry points are
 * declared in mario.h, so these may appear before the trap definitions below. */
var_t* mario_get_prototype_var(vm_t* vm, var_t* obj) {
	if(obj == NULL)
		return NULL;
	if(var_is_proxy(obj))
		return proxy_get_prototype(vm, obj);   // refs=0 or NULL
	return var_get_prototype(obj);             // borrowed (persistent prototype)
}

bool mario_set_prototype_var(vm_t* vm, var_t* obj, var_t* proto) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_set_prototype(vm, obj, proto);
	var_set_prototype(obj, proto);
	return true;
}

bool mario_is_extensible_var(vm_t* vm, var_t* obj) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_is_extensible(vm, obj);
	return mario_is_extensible(obj);
}

bool mario_prevent_extensions_var(vm_t* vm, var_t* obj) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_prevent_extensions(vm, obj);
	node_t* nx = var_add(obj, OBJ_NO_EXT, var_new_bool(vm, true));
	if(nx != NULL) { nx->invisable = 1; nx->be_unenumerable = 1; }
	return true;
}

bool mario_define_property_var(vm_t* vm, var_t* obj, var_t* key, var_t* desc) {
	if(obj == NULL)
		return false;
	if(var_is_proxy(obj))
		return proxy_define_property(vm, obj, key, desc);
	return mario_define_default(vm, obj, key, desc);
}

var_t* mario_gopd_var(vm_t* vm, var_t* obj, var_t* key) {
	if(obj == NULL)
		return var_new(vm);
	if(var_is_proxy(obj)) {
		var_t* d = proxy_get_own_descriptor(vm, obj, key);   // refs=0 or NULL
		return (d != NULL) ? d : var_new(vm);
	}
	return mario_gopd_default(vm, obj, key);                 // refs=0
}

var_t* mario_apply_var(vm_t* vm, var_t* func, var_t* thisArg, var_t* argsNatural) {
	if(func == NULL || !var_is_callable(func)) {
		vm_throw_type_native(vm, "TypeError", "target is not callable");
		return var_new(vm);
	}
	if(var_is_proxy(func))
		return proxy_apply(vm, func, thisArg, argsNatural);  // refs=0
	var_t* r = call_with_args(vm, thisArg, func, argsNatural);
	if(r != NULL && r->refs > 0) r->refs--;
	return (r != NULL) ? r : var_new(vm);
}

var_t* mario_construct_var(vm_t* vm, var_t* ctor, var_t* argsNatural, var_t* newTarget) {
	if(ctor == NULL) {
		vm_throw_type_native(vm, "TypeError", "target is not a constructor");
		return var_new(vm);
	}
	return construct_from_var(vm, ctor, argsNatural, newTarget);   // refs=0; proxy-aware
}

/* ---- the thirteen traps ---- */

var_t* proxy_get(vm_t* vm, var_t* p, var_t* key, var_t* receiver) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return var_new(vm); }
	var_t* fn = proxy_trap_func(p, "get");
	if(fn != NULL) {
		var_ref(fn);
		var_t* recv = (receiver != NULL) ? receiver : p;
		var_t* argv[2]; argv[0] = key; argv[1] = recv;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 2);
		var_unref(fn);
		if(res != NULL && res->refs > 0) res->refs--;
		return (res != NULL) ? res : var_new(vm);
	}
	return mario_get_var(vm, var_proxy_target(p), key, receiver);
}

bool proxy_set(vm_t* vm, var_t* p, var_t* key, var_t* value, var_t* receiver) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "set");
	if(fn != NULL) {
		var_ref(fn);
		var_t* recv = (receiver != NULL) ? receiver : p;
		var_t* argv[3]; argv[0] = key; argv[1] = value; argv[2] = recv;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 3);
		var_unref(fn);
		bool ok = (res != NULL) ? var_get_bool(res) : false;
		if(res != NULL) var_unref(res);
		if(!ok)
			vm_throw_type_native(vm, "TypeError", "proxy set trap returned false");
		return ok;
	}
	return mario_set_var(vm, var_proxy_target(p), key, value, receiver);
}

bool proxy_has(vm_t* vm, var_t* p, var_t* key) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "has");
	if(fn != NULL) {
		var_ref(fn);
		var_t* argv[1]; argv[0] = key;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 1);
		var_unref(fn);
		bool b = (res != NULL) ? var_get_bool(res) : false;
		if(res != NULL) var_unref(res);
		return b;
	}
	return mario_has_var(vm, var_proxy_target(p), key);
}

bool proxy_delete(vm_t* vm, var_t* p, var_t* key) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "deleteProperty");
	if(fn != NULL) {
		var_ref(fn);
		var_t* argv[1]; argv[0] = key;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 1);
		var_unref(fn);
		bool b = (res != NULL) ? var_get_bool(res) : true;
		if(res != NULL) var_unref(res);
		return b;
	}
	return mario_delete_var(vm, var_proxy_target(p), key);
}

var_t* proxy_own_keys(vm_t* vm, var_t* p, bool strings_only, bool enum_only) {
	(void)strings_only;
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return var_new_array(vm); }
	var_t* fn = proxy_trap_func(p, "ownKeys");
	if(fn != NULL) {
		var_ref(fn);
		var_t* res = proxy_call_trap(vm, p, fn, NULL, 0);
		var_unref(fn);
		if(res != NULL && res->refs > 0) res->refs--;
		return (res != NULL) ? res : var_new_array(vm);
	}
	return own_keys_default(vm, var_proxy_target(p), enum_only);
}

var_t* proxy_apply(vm_t* vm, var_t* p, var_t* thisArg, var_t* args) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return var_new(vm); }
	var_t* target = var_proxy_target(p);
	if(target == NULL || !var_is_callable(target)) {
		vm_throw_type_native(vm, "TypeError", "proxy target is not callable");
		return var_new(vm);
	}
	var_t* fn = proxy_trap_func(p, "apply");
	if(fn != NULL) {
		var_t* ta = (thisArg != NULL) ? thisArg : var_new(vm);
		var_t* aa = (args != NULL) ? args : var_new_array(vm);
		var_ref(fn); var_ref(ta); var_ref(aa);
		var_t* argv[2]; argv[0] = ta; argv[1] = aa;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 2);
		var_unref(fn); var_unref(ta); var_unref(aa);
		if(res != NULL && res->refs > 0) res->refs--;
		return (res != NULL) ? res : var_new(vm);
	}
	var_t* res = call_with_args(vm, thisArg, target, args);
	if(res != NULL && res->refs > 0) res->refs--;
	return (res != NULL) ? res : var_new(vm);
}

var_t* proxy_construct(vm_t* vm, var_t* p, var_t* args, var_t* newTarget) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return var_new(vm); }
	var_t* target = var_proxy_target(p);
	if(target == NULL || !var_is_callable(target)) {
		vm_throw_type_native(vm, "TypeError", "proxy target is not a constructor");
		return var_new(vm);
	}
	var_t* fn = proxy_trap_func(p, "construct");
	if(fn != NULL) {
		var_t* aa = (args != NULL) ? args : var_new_array(vm);
		var_t* nt = (newTarget != NULL) ? newTarget : p;
		var_ref(fn); var_ref(aa); var_ref(nt);
		var_t* argv[2]; argv[0] = aa; argv[1] = nt;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 2);
		var_unref(fn); var_unref(aa); var_unref(nt);
		if(res != NULL && res->refs > 0) res->refs--;
		return (res != NULL) ? res : var_new(vm);
	}
	return construct_from_var(vm, target, args, (newTarget != NULL) ? newTarget : target);
}

var_t* proxy_get_prototype(vm_t* vm, var_t* p) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return NULL; }
	var_t* fn = proxy_trap_func(p, "getPrototypeOf");
	if(fn != NULL) {
		var_ref(fn);
		var_t* res = proxy_call_trap(vm, p, fn, NULL, 0);
		var_unref(fn);
		if(res != NULL && res->type != V_OBJECT && res->type != V_NULL) {
			vm_throw_type_native(vm, "TypeError", "proxy getPrototypeOf must return an object or null");
			var_unref(res);
			return NULL;
		}
		if(res != NULL && res->refs > 0) res->refs--;
		return res;
	}
	var_t* t = var_proxy_target(p);
	return (t != NULL) ? var_get_prototype(t) : NULL;
}

bool proxy_set_prototype(vm_t* vm, var_t* p, var_t* proto) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "setPrototypeOf");
	if(fn != NULL) {
		var_t* pv = (proto != NULL) ? proto : var_new_null(vm);
		var_ref(fn); var_ref(pv);
		var_t* argv[1]; argv[0] = pv;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 1);
		var_unref(fn); var_unref(pv);
		bool b = (res != NULL) ? var_get_bool(res) : false;
		if(res != NULL) var_unref(res);
		return b;
	}
	var_t* t = var_proxy_target(p);
	if(t != NULL) var_set_prototype(t, proto);
	return true;
}

bool proxy_is_extensible(vm_t* vm, var_t* p) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "isExtensible");
	if(fn != NULL) {
		var_ref(fn);
		var_t* res = proxy_call_trap(vm, p, fn, NULL, 0);
		var_unref(fn);
		bool b = (res != NULL) ? var_get_bool(res) : true;
		if(res != NULL) var_unref(res);
		return b;
	}
	return mario_is_extensible(var_proxy_target(p));
}

bool proxy_prevent_extensions(vm_t* vm, var_t* p) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "preventExtensions");
	if(fn != NULL) {
		var_ref(fn);
		var_t* res = proxy_call_trap(vm, p, fn, NULL, 0);
		var_unref(fn);
		bool b = (res != NULL) ? var_get_bool(res) : false;
		if(res != NULL) var_unref(res);
		if(b && mario_is_extensible(var_proxy_target(p))) {
			vm_throw_type_native(vm, "TypeError", "proxy preventExtensions trap returned true but the target is extensible");
			return false;
		}
		return b;
	}
	var_t* t = var_proxy_target(p);
	if(t != NULL) {
		node_t* nx = var_add(t, OBJ_NO_EXT, var_new_bool(vm, true));
		if(nx != NULL) { nx->invisable = 1; nx->be_unenumerable = 1; }
	}
	return true;
}

bool proxy_define_property(vm_t* vm, var_t* p, var_t* key, var_t* desc) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return false; }
	var_t* fn = proxy_trap_func(p, "defineProperty");
	if(fn != NULL) {
		var_t* dv = (desc != NULL) ? desc : var_new(vm);
		var_ref(fn); var_ref(key); var_ref(dv);
		var_t* argv[2]; argv[0] = key; argv[1] = dv;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 2);
		var_unref(fn); var_unref(key); var_unref(dv);
		bool b = (res != NULL) ? var_get_bool(res) : false;
		if(res != NULL) var_unref(res);
		return b;
	}
	return mario_define_default(vm, var_proxy_target(p), key, desc);
}

var_t* proxy_get_own_descriptor(vm_t* vm, var_t* p, var_t* key) {
	if(var_proxy_is_revoked(p)) { proxy_throw_revoked(vm); return NULL; }
	var_t* fn = proxy_trap_func(p, "getOwnPropertyDescriptor");
	if(fn != NULL) {
		var_ref(fn); var_ref(key);
		var_t* argv[1]; argv[0] = key;
		var_t* res = proxy_call_trap(vm, p, fn, argv, 1);
		var_unref(fn); var_unref(key);
		if(res != NULL && res->refs > 0) res->refs--;
		return res;
	}
	return mario_gopd_default(vm, var_proxy_target(p), key);
}

static inline void handle_include(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		return;
	}
	do_include(vm, var_get_str(v));
	var_unref(v);
}


static inline void handle_throw(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	vm_throw_truncate(vm); // the thrown value is on top; drop operands the interrupted expression leaked
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			mario_printf("Error: 'throw' not in any try...catch!\n");
			vm_terminate(vm);
			break;
		}
		if(sc->is_try) {
			/* Consume this try: the catch handler is entered now, so a throw raised
			 * from within its own catch body must NOT re-match the same scope (the
			 * scope lives until INSTR_TRY_END, after the catch body). Without this
			 * the second throw jumps back to the same catch forever -> hang. */
			sc->is_try = false;
			vm->pc = sc->pc;
			break;
		}
		vm_pop_scope(vm);
	}
}

static inline void handle_catch(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		return;
	}
	var_t* sc_var = vm_get_scope_var(vm);
	var_add(sc_var, s, v);
	var_unref(v);
}

/* Instruction handler table - initialized at startup */
static instr_handler_t instr_table[INSTR_MAX];
static bool instr_table_initialized = false;

/* Initialize instruction handler table */
static void init_instr_table(void) {
	if(instr_table_initialized) return;

	/* Initialize all entries to handle_nil (safe default) */
	for(int i = 0; i < INSTR_MAX; i++) {
		instr_table[i] = handle_nil;
	}

	/* Register instruction handlers */
	instr_table[INSTR_NIL] = handle_nil;
	instr_table[INSTR_STRICT] = handle_strict;
	instr_table[INSTR_VAR] = handle_var;
	instr_table[INSTR_CONST] = handle_const;
	instr_table[INSTR_LOAD] = handle_load;
	instr_table[INSTR_GET] = handle_get;
	instr_table[INSTR_GETW] = handle_getw;
	instr_table[INSTR_ASIGN] = handle_asign;

	instr_table[INSTR_INT] = handle_int;
	instr_table[INSTR_INT_S] = handle_int_s;
	instr_table[INSTR_FLOAT] = handle_float;
	instr_table[INSTR_INT64] = handle_int64;
	instr_table[INSTR_FLOAT64] = handle_float64;
	instr_table[INSTR_STR] = handle_str;
	instr_table[INSTR_BIGINT] = handle_bigint;
	instr_table[INSTR_ARRAY_AT] = handle_array_at;
	instr_table[INSTR_ARRAY_AT_W] = handle_array_at_w;
	instr_table[INSTR_ARRAY_AT_M] = handle_array_at_m;
	instr_table[INSTR_ARRAY] = handle_obj;
	instr_table[INSTR_ARRAY_END] = handle_obj_end;
	instr_table[INSTR_SAFE_VAR] = handle_const;

	instr_table[INSTR_FUNC] = handle_func;
	instr_table[INSTR_FUNC_GET] = handle_func;
	instr_table[INSTR_FUNC_SET] = handle_func;
	instr_table[INSTR_FUNC_ARROW] = handle_func;
	instr_table[INSTR_FUNC_GEN] = handle_func;
	instr_table[INSTR_CALL] = handle_call;
	instr_table[INSTR_CALLO] = handle_call;
	instr_table[INSTR_CALLX] = handle_callx;
	instr_table[INSTR_CALLXO] = handle_callxo;
	instr_table[INSTR_TAG_RAW] = handle_tag_raw;
	instr_table[INSTR_CLASS] = handle_class;
	instr_table[INSTR_CLASS_END] = handle_class_end;
	instr_table[INSTR_MEMBER] = handle_member;
	instr_table[INSTR_MEMBERN] = handle_member;
	instr_table[INSTR_FUNC_STC] = handle_func;

	instr_table[INSTR_NOT] = handle_not;
	instr_table[INSTR_MULTI] = handle_math;
	instr_table[INSTR_DIV] = handle_math;
	instr_table[INSTR_MOD] = handle_math;
	instr_table[INSTR_PLUS] = handle_math;
	instr_table[INSTR_MINUS] = handle_math;
	instr_table[INSTR_NEG] = handle_neg;
	instr_table[INSTR_POS] = handle_pos;
	instr_table[INSTR_BNOT] = handle_bnot;
	instr_table[INSTR_PPLUS] = handle_pplus;
	instr_table[INSTR_MMINUS] = handle_mminus;
	instr_table[INSTR_PPLUS_PRE] = handle_pplus_pre;
	instr_table[INSTR_MMINUS_PRE] = handle_mminus_pre;
	instr_table[INSTR_LSHIFT] = handle_math;
	instr_table[INSTR_RSHIFT] = handle_math;

	instr_table[INSTR_EQ] = handle_compare;
	instr_table[INSTR_NEQ] = handle_compare;
	instr_table[INSTR_LEQ] = handle_compare;
	instr_table[INSTR_GEQ] = handle_compare;
	instr_table[INSTR_GRT] = handle_compare;
	instr_table[INSTR_LES] = handle_compare;
	instr_table[INSTR_PLUSEQ] = handle_math;
	instr_table[INSTR_MINUSEQ] = handle_math;
	instr_table[INSTR_MULTIEQ] = handle_math;
	instr_table[INSTR_DIVEQ] = handle_math;
	instr_table[INSTR_MODEQ] = handle_math;

	instr_table[INSTR_AAND] = handle_logic;
	instr_table[INSTR_OOR] = handle_logic;
	instr_table[INSTR_AND] = handle_math;
	instr_table[INSTR_OR] = handle_math;
	instr_table[INSTR_XOR] = handle_math;
	instr_table[INSTR_URSHIFT] = handle_math;
	instr_table[INSTR_BITANDEQ] = handle_math;
	instr_table[INSTR_BITOREQ] = handle_math;
	instr_table[INSTR_BITXOREQ] = handle_math;
	instr_table[INSTR_LSHIFTEQ] = handle_math;
	instr_table[INSTR_RSHIFTEQ] = handle_math;
	instr_table[INSTR_URSHIFTEQ] = handle_math;

	instr_table[INSTR_TEQ] = handle_compare;
	instr_table[INSTR_NTEQ] = handle_compare;
	instr_table[INSTR_TYPEOF] = handle_typeof;

	instr_table[INSTR_BREAK] = handle_break;
	instr_table[INSTR_CONTINUE] = handle_continue;
	instr_table[INSTR_RETURN] = handle_return;
	instr_table[INSTR_RETURNV] = handle_return;

	instr_table[INSTR_NJMP] = handle_njmp;
	instr_table[INSTR_JMPB] = handle_jmpb;
	instr_table[INSTR_NJMPB] = handle_njmp;
	instr_table[INSTR_JMP] = handle_jmp;

	instr_table[INSTR_TRUE] = handle_true;
	instr_table[INSTR_FALSE] = handle_false;
	instr_table[INSTR_NULL] = handle_null;
	instr_table[INSTR_UNDEF] = handle_undef;

	instr_table[INSTR_NEW] = handle_new;

	instr_table[INSTR_CACHE] = handle_cache;
	instr_table[INSTR_NCACHE] = handle_ncache;

	instr_table[INSTR_POP] = handle_pop;

	instr_table[INSTR_OBJ] = handle_obj;
	instr_table[INSTR_OBJ_END] = handle_obj_end;
	instr_table[INSTR_SET_PROTO] = handle_set_proto;
	instr_table[INSTR_GET_ITER] = handle_get_iter;

	instr_table[INSTR_ARR_SPREAD] = handle_arr_spread;
	instr_table[INSTR_OBJ_SPREAD] = handle_obj_spread;
	instr_table[INSTR_CALL_SPREAD] = handle_call_spread;
	instr_table[INSTR_CALLO_SPREAD] = handle_call_spread;
	instr_table[INSTR_CALLX_SPREAD] = handle_call_spread;
	instr_table[INSTR_CALLXO_SPREAD] = handle_call_spread;
	instr_table[INSTR_NEW_SPREAD] = handle_new_spread;
	instr_table[INSTR_MEMBERV] = handle_memberv;
	instr_table[INSTR_POW] = handle_math;
	instr_table[INSTR_POWEQ] = handle_math;
	instr_table[INSTR_OPT_GET] = handle_opt_get;
	instr_table[INSTR_NULLISH] = handle_nullish;
	instr_table[INSTR_SCOR] = handle_logic_sc;
	instr_table[INSTR_SCAND] = handle_logic_sc;
	instr_table[INSTR_OREQ] = handle_logic_assign;
	instr_table[INSTR_ANDEQ] = handle_logic_assign;
	instr_table[INSTR_NULLISHEQ] = handle_logic_assign;

	instr_table[INSTR_BLOCK] = handle_block;
	instr_table[INSTR_BLOCK_END] = handle_block_end;
	instr_table[INSTR_LOOP] = handle_block;
	instr_table[INSTR_LOOP_END] = handle_block_end;
	instr_table[INSTR_TRY] = handle_block;
	instr_table[INSTR_TRY_END] = handle_block_end;
	instr_table[INSTR_SWITCH] = handle_block;
	instr_table[INSTR_SWITCH_END] = handle_block_end;

	instr_table[INSTR_THROW] = handle_throw;
	instr_table[INSTR_CATCH] = handle_catch;
	instr_table[INSTR_INSTOF] = handle_instof;
	instr_table[INSTR_DELETE] = handle_delete;
	instr_table[INSTR_DELETE_AT] = handle_delete_at;
	instr_table[INSTR_DELETE_VAR] = handle_delete_var;
	instr_table[INSTR_IN] = handle_in;

	instr_table[INSTR_YIELD] = handle_yield;
	instr_table[INSTR_YIELD_STAR] = handle_yield;

	instr_table[INSTR_INCLUDE] = handle_include;

	instr_table_initialized = true;
}

bool vm_run(vm_t* vm) {
	/* Initialize instruction table on first run */
	if(!instr_table_initialized) {
		init_instr_table();
	}

	register PC code_size = vm->bc.cindex;
	register PC* code = vm->bc.code_buf;

	do {
		/* Opportunistic gc safe point: between instructions the value stack and
		 * scope stack hold every live var, so a collection here cannot sweep a
		 * var a C frame still references bare (native_Array_sort et al.). */
		if(vm->gc.gc_pending && vm->gc.gc_defer == 0 && !vm->gc.is_doing_gc) {
			vm->gc.gc_pending = false;
			gc(vm, false);
		}
		register PC ins = code[vm->pc++];
		register opr_code_t instr = OP(ins);
		register uint32_t offset = OFF(ins);

		if(instr == INSTR_END) {
			break;
		}

		/* Table-based instruction dispatch */
		instr_table[instr](vm, ins, instr, offset);

		/* Page-independent service cadence (see mario.h): one increment+compare
		 * per dispatch when armed, nothing when step_interval is 0. The hook may
		 * set vm->terminated to abort the whole nested run. */
		if(vm->step_interval != 0 && ++vm->step_count >= vm->step_interval) {
			vm->step_count = 0;
			if(vm->on_step != NULL)
				vm->on_step(vm, vm->on_step_data);
		}

		/* Handle return instructions */
		if(instr == INSTR_RETURN || instr == INSTR_RETURNV) {
			return true;
		}

		/* A yield suspended the generator body this vm_run() frame is running:
		 * hand control back to gen_resume() with the stacks left as they are. */
		if(vm->yielded && (instr == INSTR_YIELD || instr == INSTR_YIELD_STAR)) {
			return false;
		}
	}
	while(vm->pc < code_size && !vm->terminated);
	return false;
}

bool vm_load(vm_t* vm, const char* s) {
	if(vm->compiler == NULL)
		return false;

	if(vm->bc.cindex > 0) {
		//vm->bc.cindex--;
		vm->pc = vm->bc.cindex;
	}
	return vm->compiler(&vm->bc, s);
}

bool vm_load_run(vm_t* vm, const char* s) {
	bool ret = false;
	if(vm_load(vm, s)) {
		vm_run(vm);
		ret = true;
	}
	return ret;
}

bool vm_load_run_native(vm_t* vm, const char* s) {
	bool ret = false;
	PC old = vm->pc;

	if(vm_load(vm, s)) {
		vm_run(vm);
		ret = true;
	}

	vm->pc = old;
	return ret;
}

typedef struct st_native_init {
	void (*func)(void*);
	void *data;
} native_init_t;

void vm_close(vm_t* vm) {
	vm->terminated = true;
	if(vm->on_close != NULL)
		vm->on_close(vm);

	int i;
	for(i=0; i<vm->close_natives.size; i++) {
		native_init_t* it = (native_init_t*)array_get(&vm->close_natives, i);
		it->func(it->data);
	}
	array_clean(&vm->close_natives, NULL);
	var_unref(vm->builtin_vars.var_true);
	var_unref(vm->builtin_vars.var_false);
	var_unref(vm->builtin_vars.var_null);
	/* freed above; NULL them so the final gc() below never marks
	 * through freed memory. */
	vm->builtin_vars.var_true = NULL;
	vm->builtin_vars.var_false = NULL;
	vm->builtin_vars.var_null = NULL;

	var_cache_free(vm);
	load_ncache_free(vm);

	// Pop and free all scopes
	while(vm->scope_stack_top > 0) {
		scope_t* sc = vm->scope_stack[vm->scope_stack_top - 1];
		vm->scope_stack_top--;
		scope_free(sc);
	}
	array_clean(&vm->init_natives, NULL);
	array_clean(&vm->included, (free_func_t)mstr_free);	

	var_unref(vm->root);
	vm->root = NULL;
	bc_release(&vm->bc);
	vm->stack_top = 0;

	weak_registry_free(vm); // Phase 6: release weak cells (unref held/callback) before the final sweep
	gc(vm, true);
	mario_free(vm);
}	

/**======native extends functions======*/

void vm_reg_init(vm_t* vm, void (*func)(void*), void* data) {
	native_init_t* it = (native_init_t*)mario_malloc(sizeof(native_init_t));
	it->func = func;
	it->data = data;
	array_add(&vm->init_natives, it);
}

void vm_reg_close(vm_t* vm, void (*func)(void*), void* data) {
	native_init_t* it = (native_init_t*)mario_malloc(sizeof(native_init_t));
	it->func = func;
	it->data = data;
	array_add(&vm->close_natives, it);
}

node_t* vm_reg_var(vm_t* vm, var_t* cls, const char* name, var_t* var, bool be_const) {
	var_t* cls_var = vm->root;
	if(cls != NULL) {
		cls_var = var_get_prototype(cls);
	}

	node_t* node = var_add(cls_var, name, var);
	node->be_const = be_const;
	node->be_unenumerable = true;
	return node;
}

/* Shared body of vm_reg_native: parse "name(a,b)" and register a native
 * function as an own member of `target`. */
static node_t* reg_native_to(vm_t* vm, var_t* target, const char* decl, native_func_t native, void* data) {
	var_t* cls_var = (target != NULL) ? target : vm->root;

	mstr_t* name = mstr_new("");
	mstr_t* arg = mstr_new("");

	func_t* func = func_new();
	func->native = native;
	func->data = data;

	const char *off = decl;
	//read name
	while(*off != '(') { 
		if(*off != ' ') //skip spaces
			mstr_add(name, *off);
		off++; 
	}
	off++; 

	while(*off != 0) {
		if(*off == ',' || *off == ')') {
			if(arg->len > 0)
				array_add_buf(&func->args, arg->cstr, arg->len+1);
			mstr_reset(arg);
		}
		else if(*off != ' ') //skip spaces
			mstr_add(arg, *off);

		off++; 
	} 
	mstr_free(arg);

	var_t* var = var_new_func(vm, func);
	node_t* node = var_add(cls_var, name->cstr, var);
	node->be_unenumerable = true;
	mstr_free(name);

	return node;
}

/* Register a native function as an OWN member of an arbitrary target var
 * (e.g. a global function object such as `Symbol`). Unlike vm_reg_native this
 * does not resolve through a class prototype. */
node_t* vm_reg_native_on(vm_t* vm, var_t* target, const char* decl, native_func_t native, void* data) {
	return reg_native_to(vm, target, decl, native, data);
}

node_t* vm_reg_native(vm_t* vm, var_t* cls, const char* decl, native_func_t native, void* data) {
	var_t* cls_var = vm->root;
	if(cls != NULL) {
		cls_var = var_get_prototype(cls);
	}
	return reg_native_to(vm, cls_var, decl, native, data);
}

node_t* vm_reg_static(vm_t* vm, var_t* cls, const char* decl, native_func_t native, void* data) {
	node_t* n = vm_reg_native(vm, cls, decl, native, data);
	func_t* func = var_get_func(n->var);
	func->is_static = true;
	return n;
}

const char* get_str(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	return v == NULL ? "" : var_get_str(v);
}

int get_int(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	return v == NULL ? 0 : var_get_int(v);
}

bool get_bool(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	return v == NULL ? 0 : var_get_bool(v);
}

float get_float(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	return v == NULL ? 0 : var_get_float(v);
}

double get_float64(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	return v == NULL ? 0 : var_get_float64(v);
}

var_t* get_obj(var_t* var, const char* name) {
	//if(strcmp(name, THIS) == 0)
	//	return var;
	node_t* n = var_find_own_member(var, name);
	if(n == NULL)
		return NULL;
	return n->var;
}

void* get_raw(var_t* var, const char* name) {
	var_t* v = get_obj(var, name);
	if(v == NULL)
		return NULL;
	return v->value;
}

var_t* get_obj_member(var_t* env, const char* name) {
	var_t* obj = get_obj(env, THIS);
	if(obj == NULL)
		return NULL;
	return var_find_own_member_var(obj, name);
}

var_t* set_obj_member(var_t* env, const char* name, var_t* var) {
	var_t* obj = get_obj(env, THIS);
	if(obj == NULL)
		return NULL;
	var_add(obj, name, var);
	return var;
}

var_t* get_func_args(var_t* env) {
	return get_obj(env, "arguments");
}

uint32_t get_func_args_num(var_t* env) {
	var_t* args = get_func_args(env);
	return var_array_size(args);
}

var_t* get_func_arg(var_t* env, uint32_t index) {
	var_t* args = get_func_args(env);
	return var_array_get_var(args, index);
}

int get_func_arg_int(var_t* env, uint32_t index) {
	return var_get_int(get_func_arg(env, index));
}

bool get_func_arg_bool(var_t* env, uint32_t index) {
	return var_get_bool(get_func_arg(env, index));
}

float get_func_arg_float(var_t* env, uint32_t index) {
	return var_get_float(get_func_arg(env, index));
}

const char* get_func_arg_str(var_t* env, uint32_t index) {
	return var_get_str(get_func_arg(env, index));
}

vm_t* vm_from(vm_t* vm) {
	vm_t* ret = vm_new(vm->compiler, vm->var_cache.size, vm->load_ncache.size);
	ret->gc.gc_trig_var_num = vm->gc.gc_trig_var_num;
	ret->gc.free_var_buffer_num = vm->gc.free_var_buffer_num;
  	vm_init(ret, vm->on_init, vm->on_close);
	return ret;
}

vm_t* vm_new(compiler_func_t compiler, uint32_t var_cache_size, uint32_t load_ncache_size) {
	if(_platform_malloc == NULL || _platform_free == NULL || _platform_out == NULL) //check platform functions
		return NULL;

	vm_t* vm = (vm_t*)mario_malloc(sizeof(vm_t));
	memset(vm, 0, sizeof(vm_t));

	vm->compiler = compiler;
	vm->var_cache.size = var_cache_size;
	vm->load_ncache.size = load_ncache_size;

	vm->terminated = false;
	vm->pc = 0;
	vm->gc.gc_trig_var_num = GC_TRIG_VAR_NUM_DEF;
	vm->gc.gc_pending = false;
	vm->gc.free_var_buffer_num = FREE_VAR_BUFFER_NUM_DEF;
	vm->stack_top = 0;

	bc_init(&vm->bc);

	vm->this_strIndex = bc_getstrindex(&vm->bc, THIS);
	vm->scope_stack_top = 0;

	var_cache_init(vm);
	load_ncache_init(vm);

	array_init(&vm->included);	
	array_init(&vm->close_natives);	
	array_init(&vm->init_natives);	
	
	vm->root = var_new_obj_no_proto(vm, NULL, NULL);
	//vm_push_scope(vm, scope_new(vm->root));

	var_ref(vm->root);
	vm->builtin_vars.var_true = var_new_bool(vm, true);
	//var_add(vm->root, "", vm->builtin_vars.var_true);
	var_ref(vm->builtin_vars.var_true);
	vm->builtin_vars.var_false = var_new_bool(vm, false);
	//var_add(vm->root, "", vm->builtin_vars.var_false);
	var_ref(vm->builtin_vars.var_false);
	vm->builtin_vars.var_null = var_new_null(vm);
	//var_add(vm->root, "", vm->builtin_vars.var_null);
	var_ref(vm->builtin_vars.var_null);

	return vm;
}

void vm_init(vm_t* vm,
		void (*on_init)(struct st_vm* vm),
		void (*on_close)(struct st_vm* vm)) {
	_done_arr_inited = false;
	vm->on_init = on_init;
	vm->on_close = on_close;
	
	if(vm->on_init != NULL)
		vm->on_init(vm);

	int i;
	for(i=0; i<vm->init_natives.size; i++) {
		native_init_t* it = (native_init_t*)array_get(&vm->init_natives, i);
		it->func(it->data);
	}
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

