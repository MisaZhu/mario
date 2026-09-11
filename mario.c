#include "mario.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

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
	snprintf(_mstr_result, STATIC_mstr_MAX-1, "%f", i);
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
	float f = 0.0;
	const char* s = str;

	if(instr == INSTR_INT) {
		if(strstr(str, "0x") != NULL) {
			/* Hex literals keep their uint32 bit pattern so bit masks such as
			 * 0xFFFFFFFF still wrap to the intended int32 value. */
			i = (uint32_t)strtoul(str, NULL, 16);
		}
		else {
			/* A decimal integer too large for int32 must not be silently
			 * truncated (9007199254740991 -> -1). JS has no int32 literal type,
			 * so promote it to a float, matching how it is represented at
			 * runtime and keeping Number.MAX_SAFE_INTEGER self-consistent. */
			long long ll = strtoll(str, NULL, 10);
			if(ll < -2147483648LL || ll > 2147483647LL) {
				instr = INSTR_FLOAT;
				f = (float)(double)ll;
			}
			else
				i = (uint32_t)(int)ll;
		}
		s = NULL;
	}
	else if(instr == INSTR_FLOAT) {
		f = (float)strtod(str, NULL);
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
	else if(instr == INSTR_FLOAT) {
		memcpy(&i, &f, sizeof(PC));
		bc_add(bc, i);
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
	if(vm->load_ncache.size == 0)
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
	
	if(node->var != NULL) {
		load_ncache_invalidate(node->var->vm, node);
	}


	if(!var_empty(node->var)) {
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
	if(vm->gc.gc_vars_num > GC_TRIG_VAR_NUM_DEF && vm->gc.gc_defer == 0)
		gc(vm, false);
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
		func->closure.func = closure_func;
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
			var_t* closure = func->closure.var;
			if(!var_empty(closure) && closure->gc_marking == false)
				gc_mark(closure, mark);
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
		if(sc != NULL)
			gc_mark(sc->var, mark);
	}
}

void var_free(void* p) {
	var_t* var = (var_t*)p;
	if(var_empty(var))
		return;

	vm_t* vm = var->vm;

	if(var->is_func) {
		func_t* func = var_get_func(var);
		if(func != NULL && func->closure.var != NULL) {
			/* Detach the closure BEFORE releasing it. A function returned out of
			 * the scope that defines it keeps two links to that same scope: the
			 * scope still owns the node holding this var, and this var's func_t
			 * owns the scope. Releasing the scope while both links are live
			 * re-enters var_free() on this very var (var_unref() frees a var whose
			 * refs are already 0), and that inner call completes the teardown -
			 * func_free()ing the func_t and queueing this var for reuse - leaving
			 * us writing through freed memory and queueing the var a second time. */
			var_t* closure = func->closure.var;
			func->closure.var = NULL;
			func->closure.func = NULL;
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
	mario_debug("done.\n");
}

static const char* get_typeof(var_t* var) {
	switch(var->type) {
		case V_UNDEF:
			return "undefined";
		case V_INT:
		case V_FLOAT:
			return "number";
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
			return var->is_func ? "function": "object";
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
	int i = (int)(*(int*)var->value);
	return i==0 ? false:true;
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
		case V_FLOAT: {
			float f = *(float*)v->value;
			return f != 0.0f && f == f; // NaN is falsy
		}
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
	if(var->type == V_FLOAT)	
		return (int)(*(float*)var->value);
	return *(int*)var->value;
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
	
	if(var->type == V_INT)	
		return (float)(*(int*)var->value);
	return *(float*)var->value;
}

inline var_t* var_set_float(var_t* var, float v) {
	var->type = V_FLOAT;
	if(var->value != NULL)
		mario_free(var->value);
	var->value = mario_malloc(sizeof(int));
	*((float*)var->value) = v;
	return var;
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
	case V_FLOAT:
		mstr_cpy(ret, mstr_from_float(var_get_float(var)));
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
				var_to_json_str(var, ret, 0);
			}
		}
		break;
	case V_BOOL:
		mstr_cpy(ret, var_get_int(var) == 1 ? "true":"false");
		break;
	case V_NULL:
		mstr_cpy(ret, "null");
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
void var_to_json_str(var_t* var, mstr_t* ret, int level) {
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
			var_to_json_str(n->var, s, level);
			mstr_append(ret, s->cstr);
			mstr_free(s);

			if (i<len-1) 
				mstr_append(ret, ", ");
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
					mstr_append(ret, ",\n");
				} else {
					first = false;
				}

				// 缩进
				append_json_spaces(ret, level);

				// 添加属性名
				mstr_add(ret, '"');
				mstr_append(ret, key);
				mstr_add(ret, '"');
				mstr_append(ret, ": ");

				// 序列化属性值
				mstr_t* value_str = mstr_new("");
				var_to_json_str(node->var, value_str, level + 1);
				mstr_append(ret, value_str->cstr);
				mstr_free(value_str);

			next_entry:
				entry = entry->next;
			}
		}

		// 如果没有属性，确保格式正确
		if (first) {
			append_json_spaces(ret, level);
		} else {
			mstr_add(ret, '\n');
			append_json_spaces(ret, level - 1);
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
	gc(vm, false);
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
	array_clean(&func->args, NULL);
	mario_free(p);
}

static var_t* var_new_func(vm_t* vm, func_t* func) {
	var_t* var = var_new_obj_no_proto(vm, NULL, NULL);
	var->is_func = 1;
	var->free_func = func_free;
	var->value = func;
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
		while(true) { //same unwinding as handle_throw
			scope_t* sc = vm_get_scope(vm);
			if(sc == NULL) {
				mario_printf("Error: uncaught exception from native function!\n");
				vm_terminate(vm);
				break;
			}
			if(sc->is_try) {
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
			return true;
		default:
			return false;
	}
}

/* Publish an arithmetic result. For a compound assignment (`x += y`) the freshly
 * built result var is installed through the lvalue's node instead of being
 * written into v1 in place: one var_t is shared by every alias of a binding
 * (`var b = a;` leaves both nodes referencing the same var) and integer literals
 * are shared through vm->var_cache, so an in-place write corrupts all of them. */
static inline void math_result(vm_t* vm, opr_code_t op, node_t* n, var_t* res) {
	if(n != NULL && is_compound_assign(op))
		node_replace(n, res); //the node takes its own reference to res.
	vm_push(vm, res);
}

static inline void math_op(vm_t* vm, opr_code_t op, var_t* v1, var_t* v2, node_t* n) {
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		return;
	}

	//ES6 exponent operator ** (right-assoc handled by compiler)
	if(op == INSTR_POW || op == INSTR_POWEQ) {
		double b = 0.0, e = 0.0;
		if(v1 != NULL && v1->value != NULL) {
			if(v1->type == V_FLOAT) b = *(float*)v1->value;
			else if(v1->type == V_INT) b = (double)(*(int*)v1->value);
		}
		if(v2 != NULL && v2->value != NULL) {
			if(v2->type == V_FLOAT) e = *(float*)v2->value;
			else if(v2->type == V_INT) e = (double)(*(int*)v2->value);
		}
		double r = pow(b, e);
		/* Keep an int result when the base is an int and the exponent is a
		 * non-negative whole number whose power still fits an int. This makes
		 * `5 ** 0` (e.g. `5 ** null`, null -> 0) the int 1, so Object.is(1, x)
		 * holds, while `2 ** 0.5` / `2 ** -1` stay floats. */
		bool integral = (v1->type == V_INT && e >= 0 && e == floor(e) && r == (double)(int)r);
		//Build a fresh result; **= must not write into v1 (see math_result()).
		math_result(vm, op, n, integral ? var_new_int(vm, (int)r) : var_new_float(vm, (float)r));
		return;
	}

	//do int
	if(v1->type == V_INT && v2->type == V_INT) {
		int i1, i2, ret = 0;
		i1 = *(int*)v1->value;
		i2 = *(int*)v2->value;

		switch(op) {
			case INSTR_PLUS: 
			case INSTR_PLUSEQ: 
				ret = (i1 + i2);
				break; 
			case INSTR_MINUS: 
			case INSTR_MINUSEQ: 
				ret = (i1 - i2);
				break; 
			case INSTR_DIV: 
			case INSTR_DIVEQ: 
				ret = (i1 / i2);
				break; 
			case INSTR_MULTI: 
			case INSTR_MULTIEQ: 
				ret = (i1 * i2);
				break; 
			case INSTR_MOD: 
			case INSTR_MODEQ: 
				ret = i1 % i2;
				break; 
			case INSTR_RSHIFT: 
				ret = i1 >> i2;
				break; 
			case INSTR_LSHIFT: 
				ret = i1 << i2;
				break; 
			case INSTR_AND: 
				ret = i1 & i2;
				break; 
			case INSTR_OR: 
				ret = i1 | i2;
				break; 
		}

		math_result(vm, op, n, var_new_int(vm, ret));
		return;
	}

	//do float - both operands must really be numeric, otherwise a string
	//operand falls into the `else //INT` arms below and has its byte buffer
	//read as a 4-byte int (e.g. `"x=" + 1.5`).
	if((v1->type == V_FLOAT || v2->type == V_FLOAT) &&
			(v1->type == V_INT || v1->type == V_FLOAT) &&
			(v2->type == V_INT || v2->type == V_FLOAT)) {
		float f1, f2, ret = 0.0;

		if(v1->type == V_FLOAT)
			f1 = *(float*)v1->value;
		else //INT
			f1 = (float) *(int*)v1->value;

		if(v2->type == V_FLOAT)
			f2 = *(float*)v2->value;
		else //INT
			f2 = (float) *(int*)v2->value;

		switch(op) {
			case INSTR_PLUS: 
			case INSTR_PLUSEQ: 
				ret = (f1 + f2);
				break; 
			case INSTR_MINUS: 
			case INSTR_MINUSEQ: 
				ret = (f1 - f2);
				break; 
			case INSTR_DIV: 
			case INSTR_DIVEQ: 
				ret = (f1 / f2);
				break; 
			case INSTR_MULTI: 
			case INSTR_MULTIEQ: 
				ret = (f1 * f2);
				break; 
		}

		/* Always build a fresh var: writing the float back into v1 would leave a
		 * V_INT lvalue holding float bits (e.g. `var x = 1; x += 0.5;`). */
		math_result(vm, op, n, var_new_float(vm, ret));
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
    
	//do int
	if(v1->type == V_INT && v2->type == V_INT) {
		register int i1, i2;
		i1 = *(int*)v1->value;
		i2 = *(int*)v2->value;

		bool i = false;
		switch(op) {
			case INSTR_EQ: 
			case INSTR_TEQ:
				i = (i1 == i2);
				break; 
			case INSTR_NEQ: 
			case INSTR_NTEQ:
				i = (i1 != i2);
				break; 
			case INSTR_LES: 
				i = (i1 < i2);
				break; 
			case INSTR_GRT: 
				i = (i1 > i2);
				break; 
			case INSTR_LEQ: 
				i = (i1 <= i2);
				break; 
			case INSTR_GEQ: 
				i = (i1 >= i2);
				break; 
		}
		if(i)
			vm_push(vm, vm->builtin_vars.var_true);
		else
			vm_push(vm, vm->builtin_vars.var_false);
		return;
	}

	
	register float f1, f2;
	if(v1->value == NULL)
		f1 = 0.0;
	else if(v1->type == V_FLOAT)
		f1 = *(float*)v1->value;
	else if(v1->type == V_INT)
		f1 = (float) *(int*)v1->value;
	else //non-numeric (string/bool/null): f1 is unused outside the numeric branch.
		f1 = 0.0;

	if(v2->value == NULL)
		f2 = 0.0;
	else if(v2->type == V_FLOAT)
		f2 = *(float*)v2->value;
	else if(v2->type == V_INT)
		f2 = (float) *(int*)v2->value;
	else //non-numeric (string/bool/null): f2 is unused outside the numeric branch.
		f2 = 0.0;

	bool i = false;
	if(v1->type == v2->type || 
			((v1->type == V_INT || v1->type == V_FLOAT) &&
			(v2->type == V_INT || v2->type == V_FLOAT))) {
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
		else if(v1->type == V_INT || v1->type == V_FLOAT) {
			switch(op) {
				case INSTR_EQ: 
				case INSTR_TEQ:
					i = (f1 == f2);
					break; 
				case INSTR_NEQ: 
				case INSTR_NTEQ:
					i = (f1 != f2);
					break; 
				case INSTR_LES: 
					i = (f1 < f2);
					break; 
				case INSTR_GRT: 
					i = (f1 > f2);
					break; 
				case INSTR_LEQ: 
					i = (f1 <= f2);
					break; 
				case INSTR_GEQ: 
					i = (f1 >= f2);
					break; 
			}
		}
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
	const char* pos = strchr(full, '$');
	int args_num = 0;
	if(pos != NULL) {
		args_num = atoi(pos+1);
		if(name != NULL)
			mstr_ncpy(name, full, (uint32_t)(pos-full));	
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
	if(instr == INSTR_LOOP) {
		sc->is_loop = true;
		sc->pc_start = vm->pc+1;
		sc->pc = vm->pc+2;
	}
	else if(instr == INSTR_TRY) {
		sc->is_try = true;
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
		if(sc->is_loop) {
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
	if(v->type == V_INT) {
		int n = *(int*)v->value;
		/* -0 must be a distinct negative zero (Object.is(0,-0) === false), which
		 * int32 cannot represent. Negating integer 0 yields float -0.0. */
		if(n == 0)
			vm_push(vm, var_new_float(vm, -0.0f));
		else
			vm_push(vm, var_new_int(vm, -n));
	}
	else if(v->type == V_FLOAT) {
		float n = *(float*)v->value;
		n = -n;
		vm_push(vm, var_new_float(vm, n));
	}
	var_unref(v);
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
		case V_FLOAT:
			vm_push(vm, var_new_float(vm, *(float*)v->value));
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
				vm_push(vm, var_new_float(vm, (float)NAN)); // trailing junk -> NaN
			else if(d == (double)(int)d)
				vm_push(vm, var_new_int(vm, (int)d));
			else
				vm_push(vm, var_new_float(vm, (float)d));
			break;
		}
		default: // V_UNDEF, V_OBJECT
			if(v->type == V_OBJECT) {
				/* ES6 Symbol.toPrimitive(number) / numeric coercion. */
				var_t* p = vm_to_primitive(vm, v, "number");
				if(p != NULL) {
					if(p->type == V_INT)
						vm_push(vm, var_new_int(vm, var_get_int(p)));
					else if(p->type == V_FLOAT)
						vm_push(vm, var_new_float(vm, var_get_float(p)));
					else if(p->type == V_BOOL)
						vm_push(vm, var_new_int(vm, var_get_bool(p) ? 1 : 0));
					else if(p->type == V_STRING) {
						const char* ps = var_get_str(p);
						char* end = NULL;
						double d = (ps != NULL && *ps != 0) ? strtod(ps, &end) : 0.0;
						if(d == (double)(int)d)
							vm_push(vm, var_new_int(vm, (int)d));
						else
							vm_push(vm, var_new_float(vm, (float)d));
					}
					else
						vm_push(vm, var_new_float(vm, (float)NAN));
					var_unref(p);
					var_unref(v);
					return;
				}
			}
			vm_push(vm, var_new_float(vm, (float)NAN));
			break;
	}
	var_unref(v);
}

static inline void handle_not(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	bool i = false;
	if(v->type == V_UNDEF || *(int*)v->value == 0)
		i = true;
	var_unref(v);
	vm_push(vm, i ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);
}

static inline void handle_logic(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	bool r = false;
	int i1 = *(int*)v1->value;
	int i2 = *(int*)v2->value;

	if(instr == INSTR_AAND)
		r = (i1 != 0) && (i2 != 0);
	else
		r = (i1 != 0) || (i2 != 0);
	vm_push(vm, r ? vm->builtin_vars.var_true : vm->builtin_vars.var_false);

	var_unref(v1);
	var_unref(v2);
}

static inline void handle_math(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	node_t* n = vm_peek_node(vm); //the lvalue binding, used by the compound assignments.
	var_t* v1 = vm_pop2(vm);
	/* Popping leaves v1/v2 - and the result math_op() builds - invisible to gc(),
	 * and math_result()'s node_replace() drops the lvalue's old var, which can
	 * start a collection that sweeps them. See vm_step_op(). */
	vm->gc.gc_defer++;
	math_op(vm, instr, v1, v2, n);
	var_unref(v1);
	var_unref(v2);
	vm->gc.gc_defer--;
}

/* Build a NEW var holding `v` stepped by `step`. The old var is never written to
 * in place - see math_result() for why that would corrupt aliases and cached
 * integer literals. */
static inline var_t* var_step(vm_t* vm, var_t* v, int step) {
	if(v->type == V_FLOAT)
		return var_new_float(vm, *(float*)v->value + (float)step);
	return var_new_int(vm, *(int*)v->value + step);
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
	var_t* nv = var_step(vm, v, step);
	var_ref(nv); //our own reference to nv, dropped at the end (as handle_asign() does).
	var_t* res = prefix ? nv : v;
	var_ref(res); //keep the result alive: node_replace() releases the node's old var.
	if(n != NULL)
		node_replace(n, nv); //write the new value back through the binding.

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
	if(v->type == V_INT || v->type == V_FLOAT) {
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
		f->closure.func = cp->scope_func;
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

static inline void handle_str(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	register PC* code = vm->bc.code_buf;
	const char* s = bc_getstr(&vm->bc, offset);
	var_t* v = var_new_str(vm, s);
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

	if(func != NULL && !func->is_func) {
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

	if(func != NULL && func->is_func) {
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

	if(func != NULL && func->is_func) {
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
	f->closure.func = sc->func;
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
				f->closure.func = (s != NULL && s->is_func) ? s->func : NULL;
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

	if(func != NULL && !func->is_func) {
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

static inline void handle_array_at(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v2 = vm_pop2(vm);
	var_t* v1 = vm_pop2(vm);
	if(v1 == NULL || v2 == NULL) {
		vm_push(vm, var_new(vm));
		if(v1 != NULL) var_unref(v1);
		if(v2 != NULL) var_unref(v2);
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

static inline void handle_include(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	var_t* v = vm_pop2(vm);
	if(v == NULL) {
		return;
	}
	do_include(vm, var_get_str(v));
	var_unref(v);
}


static inline void handle_throw(vm_t* vm, PC ins, opr_code_t instr, uint32_t offset) {
	while(true) {
		scope_t* sc = vm_get_scope(vm);
		if(sc == NULL) {
			mario_printf("Error: 'throw' not in any try...catch!\n");
			vm_terminate(vm);
			break;
		}
		if(sc->is_try) {
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
	instr_table[INSTR_STR] = handle_str;
	instr_table[INSTR_ARRAY_AT] = handle_array_at;
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
	instr_table[INSTR_OREQ] = handle_logic_assign;
	instr_table[INSTR_ANDEQ] = handle_logic_assign;
	instr_table[INSTR_NULLISHEQ] = handle_logic_assign;

	instr_table[INSTR_BLOCK] = handle_block;
	instr_table[INSTR_BLOCK_END] = handle_block_end;
	instr_table[INSTR_LOOP] = handle_block;
	instr_table[INSTR_LOOP_END] = handle_block_end;
	instr_table[INSTR_TRY] = handle_block;
	instr_table[INSTR_TRY_END] = handle_block_end;

	instr_table[INSTR_THROW] = handle_throw;
	instr_table[INSTR_CATCH] = handle_catch;
	instr_table[INSTR_INSTOF] = handle_instof;

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
		register PC ins = code[vm->pc++];
		register opr_code_t instr = OP(ins);
		register uint32_t offset = OFF(ins);

		if(instr == INSTR_END) {
			break;
		}

		/* Table-based instruction dispatch */
		instr_table[instr](vm, ins, instr, offset);

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
	bc_release(&vm->bc);
	vm->stack_top = 0;

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

