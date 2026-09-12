#ifndef MARIO_H
#define MARIO_H

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**====== platform porting functions.======*/
extern void* (*_platform_malloc)(uint32_t size);
extern void  (*_platform_free)(void* p);
extern void  (*_platform_out)(const char*);

/**====== memory functions.======*/

extern void* mario_malloc(uint32_t size);
extern void  mario_free(void* p);
extern void* mario_realloc(void* p, uint32_t old_size, uint32_t new_size);

/**====== debug functions.======*/
void        mario_debug(const char *format, ...);
void        mario_printf(const char *format, ...);

/**====== array functions. ======*/
#define STATIC_mstr_MAX 48
typedef void (*free_func_t)(void* p);

typedef struct st_array {
	void**     items;
	uint32_t   max: 16;
	uint32_t   size: 16;
} m_array_t;

m_array_t*  array_new(void);
void        array_free(m_array_t* array, free_func_t fr);
void        array_init(m_array_t* array);
void        array_add(m_array_t* array, void* item);
void        array_add_head(m_array_t* array, void* item);
void*       array_add_buf(m_array_t* array, void* s, uint32_t sz);
void*       array_get(m_array_t* array, uint32_t index);
void*       array_set(m_array_t* array, uint32_t index, void* p);
void*       array_tail(m_array_t* array);
void*       array_head(m_array_t* array);
void*       array_remove(m_array_t* array, uint32_t index);
void        array_del(m_array_t* array, uint32_t index, free_func_t fr);
void        array_remove_all(m_array_t* array);
void        array_clean(m_array_t* array, free_func_t fr);
#define array_tail(array) (((array)->items == NULL || (array)->size == 0) ? NULL: (array)->items[(array)->size-1]);

/**====== hash map functions ======*/

// Hash map entry structure
typedef struct st_hash_entry {
	char*               key;
	void*               value;
	struct st_hash_entry* next;
} hash_entry_t;

// Hash map structure
typedef struct st_hash_map {
	hash_entry_t**      buckets;
	uint32_t            size;
	uint32_t            capacity;
	uint32_t            load_factor_num;   // 负载因子分子
	uint32_t            load_factor_den;   // 负载因子分母
} hash_map_t;

// Hash map functions
hash_map_t* hash_map_new(void);
void        hash_map_free(hash_map_t* map, free_func_t free_key, free_func_t free_value);
void        hash_map_init(hash_map_t* map);
void        hash_map_add(hash_map_t* map, const char* key, void* value);
void*       hash_map_get(hash_map_t* map, const char* key);
void*       hash_map_remove(hash_map_t* map, const char* key);
uint32_t    hash_map_size(hash_map_t* map);
void        hash_map_clean(hash_map_t* map, free_func_t free_key, free_func_t free_value);
void        hash_map_iterate(hash_map_t* map, void (*callback)(const char* key, void* value, void* user_data), void* user_data);

/**====== string functions. ======*/

typedef struct st_mstr {
	char*               cstr;
	uint32_t            max: 16;
	uint32_t            len: 16;
} mstr_t;

void        mstr_reset(mstr_t* str);
char*       mstr_ncpy(mstr_t* str, const char* src, uint32_t l);
char*       mstr_cpy(mstr_t* str, const char* src);
mstr_t*     mstr_new(const char* s);
mstr_t*     mstr_new_by_size(uint32_t sz);
char*       mstr_append(mstr_t* str, const char* src);
char*       mstr_add(mstr_t* str, char c);
char*       mstr_add_int(mstr_t* str, int i, int base);
char*       mstr_add_float(mstr_t* str, float f);
void        mstr_free(mstr_t* str);
const char* mstr_from_int(int i, int base);
const char* mstr_from_float(float f);
const char* mstr_from_int64(int64_t i, int base);
const char* mstr_from_float64(double d);
const char* mstr_from_bool(bool b);
int         mstr_to_int(const char* str);
float       mstr_to_float(const char* str);
void        mstr_split(const char* str, char c, m_array_t* array);
int         mstr_to(const char* str, char c, mstr_t* res, bool skipspace);

/**====== bignum (ES2020 BigInt) functions. ======*/

/* Arbitrary-precision signed integer: a little-endian base-2^32 magnitude held
 * in `limbs` (limbs[0] is the least significant) with a separate sign in
 * {-1,0,+1}. Zero is always sign==0, len==0. Backs the V_BIGINT tag: a bigint
 * var's `value` points at a bignum_t and its `free_func` is bn_free. Hand-rolled
 * (no external deps); magnitudes grow on demand. */
typedef struct st_bignum {
	int                 sign;   // -1, 0, +1
	uint32_t            len;    // number of limbs in use (high limbs never zero)
	uint32_t            cap;    // number of limbs allocated
	uint32_t*           limbs;  // little-endian base-2^32 magnitude
} bignum_t;

bignum_t*   bn_new(void);
void        bn_free(void* p);                  // free_func_t-compatible (frees limbs + struct)
bignum_t*   bn_clone(const bignum_t* b);
bignum_t*   bn_from_int64(int64_t v);
bignum_t*   bn_from_uint64(uint64_t v);
bignum_t*   bn_from_double(double d);          // d must be finite and integral
bignum_t*   bn_from_string(const char* s, int radix); // radix 0: auto-detect 0x/0b/0o/decimal
void        bn_to_mstr(const bignum_t* b, int radix, mstr_t* out); // appends "[sign]digits"
int64_t     bn_to_int64(const bignum_t* b);    // low 64 bits, reinterpreted signed
double      bn_to_double(const bignum_t* b);   // nearest double (may lose precision)
bool        bn_is_zero(const bignum_t* b);
int         bn_cmp(const bignum_t* a, const bignum_t* b);      // -1 / 0 / +1
int         bn_cmp_double(const bignum_t* a, double d);        // exact for integral d in int64 range
bignum_t*   bn_add(const bignum_t* a, const bignum_t* b);
bignum_t*   bn_sub(const bignum_t* a, const bignum_t* b);
bignum_t*   bn_mul(const bignum_t* a, const bignum_t* b);
bignum_t*   bn_div(const bignum_t* a, const bignum_t* b);      // truncating; NULL if b==0
bignum_t*   bn_mod(const bignum_t* a, const bignum_t* b);      // sign of dividend; NULL if b==0
bignum_t*   bn_pow(const bignum_t* base, const bignum_t* exp); // NULL if exp<0 (JS RangeError)
bignum_t*   bn_neg(const bignum_t* a);
bignum_t*   bn_and(const bignum_t* a, const bignum_t* b);      // infinite two's complement
bignum_t*   bn_or(const bignum_t* a, const bignum_t* b);
bignum_t*   bn_xor(const bignum_t* a, const bignum_t* b);
bignum_t*   bn_shl(const bignum_t* a, uint32_t bits);
bignum_t*   bn_shr(const bignum_t* a, uint32_t bits);          // arithmetic (floor) shift right
bignum_t*   bn_asIntN(uint32_t bits, const bignum_t* a);
bignum_t*   bn_asUintN(uint32_t bits, const bignum_t* a);

/**====== MARIO_BC ======*/

//bytecode
typedef uint32_t PC;
typedef uint16_t opr_code_t;
typedef struct st_bytecode {
	PC                  cindex;
	m_array_t           mstr_table;
	PC                 *code_buf;
	uint32_t            buf_size;
} bytecode_t;

/*
32bits bytecode:
+----------------------------------+
| 4bits  | 8bits    | 20bits       |
|----------------------------------|
| OPTION | OPR_CODE | OFFSET/VALUE |
+----------------------------------+
*/

#define ILLEGAL_PC 0xFFFFFFFF
#define INSTR_OPT_CACHE	 0x80000000
#define OFF_MASK 0x0FFFFF
#define INS(ins, off) (((((int32_t)ins) << 20) & 0xFFF00000) | ((off) & OFF_MASK))
#define OP(ins) (((ins) >>20) & 0xFF)
#define OFF(ins) ((ins) & OFF_MASK)

#define INSTR_NIL          0x000 // NIL           : Do nothing.

#define INSTR_VAR          0x001 // VAR x         : declare var x
#define INSTR_CONST        0x002 // CONST x       : declare const x
#define INSTR_LOAD         0x003 // LOAD x        : load and push x 
#define INSTR_ASIGN        0x004 // ASIGN         : =
#define INSTR_STORE        0x005 // STORE x       : pop and store to x
#define INSTR_GET          0x006 // getfield
#define INSTR_INT          0x007 // INT int       : push int
#define INSTR_FLOAT        0x008 // FLOAT float   : push float 
#define INSTR_STR          0x009 // STR "str"     : push str
#define INSTR_ARRAY_AT     0x00A // ARRAT         : get array element at
#define INSTR_ARRAY        0x00B // ARRAY         : array start
#define INSTR_ARRAY_END    0x00C // ARRAY_END     : array end
#define INSTR_INT_S        0x00D // SHORT_INT int : push short int
#define INSTR_SAFE_VAR     0x00E // SAFE_VAR x    : declare safe_var x
#define INSTR_FUNC         0x00F // FUNC x        : function definetion x

#define INSTR_FUNC_GET     0x010 // GET FUNC x    : class get function definetion x
#define INSTR_FUNC_SET     0x011 // SET FUNC x    : class set function definetion x
#define INSTR_CALL         0x012 // CALL x        : call function x and push res
#define INSTR_CALLO        0x013 // CALL obj.x    : call object member function x and push res
#define INSTR_CLASS        0x014 // class         
#define INSTR_CLASS_END    0x015 // class end            
#define INSTR_MEMBER       0x016 // member without name
#define INSTR_MEMBERN      0x017 // : member with name
#define INSTR_EXTENDS      0x018 // : class extends
#define INSTR_FUNC_STC     0x019 // ST FUNC x     : static function definetion x
#define INSTR_NOT          0x01A // NOT           : !
#define INSTR_MULTI        0x01B // MULTI         : *
#define INSTR_DIV          0x01C // DIV           : /
#define INSTR_MOD          0x01D // MOD           : %
#define INSTR_PLUS         0x01E // PLUS          : + 
#define INSTR_MINUS        0x01F // MINUS         : - 

#define INSTR_NEG          0x020 // NEG           : negate -
#define INSTR_PPLUS        0x021 // PPLUS         : x++
#define INSTR_MMINUS       0x022 // MMINUS        : x--
#define INSTR_PPLUS_PRE    0x023 // PPLUS         : ++x
#define INSTR_MMINUS_PRE   0x024 // MMINUS        : --x
#define INSTR_LSHIFT       0x025 // LSHIFT        : <<
#define INSTR_RSHIFT       0x026 // RSHIFT        : >>
#define INSTR_URSHIFT      0x027 // URSHIFT       : >>>
#define INSTR_EQ           0x028 // EQ            : ==
#define INSTR_NEQ          0x029 // NEQ           : !=
#define INSTR_LEQ          0x02A // LEQ           : <=
#define INSTR_GEQ          0x02B // GEQ           : >=
#define INSTR_GRT          0x02C // GRT           : >
#define INSTR_LES          0x02D // LES           : <

#define INSTR_PLUSEQ       0x02E // +=
#define INSTR_MINUSEQ      0x02F // -=    
#define INSTR_MULTIEQ      0x030 // *=
#define INSTR_DIVEQ        0x031 // /=
#define INSTR_MODEQ        0x032 // %=
#define INSTR_AAND         0x033 // AAND          : &&
#define INSTR_OOR          0x034 // OOR           : ||
#define INSTR_OR           0x035 // OR            : |
#define INSTR_XOR          0x036 // XOR           : ^
#define INSTR_AND          0x037 // AND           : &
#define INSTR_TEQ          0x038 // TEQ           : ===
#define INSTR_NTEQ         0x039 // NTEQ          : !==
#define INSTR_TYPEOF       0x03A // TYPEOF        : typeof

#define INSTR_BREAK        0x03B // break
#define INSTR_CONTINUE     0x03C // continue
#define INSTR_RETURN       0x03D // return none value
#define INSTR_RETURNV      0x03E // return with value
#define INSTR_NJMP         0x03F // NJMP x        : Condition not JMP offset x 

#define INSTR_JMPB         0x040 // JMP back x    : JMP back offset x  
#define INSTR_NJMPB        0x041 // NJMP back x   : Condition not JMP back offset x 
#define INSTR_JMP          0x042 // JMP x         : JMP offset x  
#define INSTR_TRUE         0x043 // true
#define INSTR_FALSE        0x044 // false
#define INSTR_NULL         0x045 // null
#define INSTR_UNDEF        0x046 // undefined
#define INSTR_NEW          0x047 // new
#define INSTR_CACHE        0x048 // CACHE index   : load cache at 'index' and push 
#define INSTR_NCACHE       0x049 // NCACHE index   : load cache at 'index' and push 
#define INSTR_POP          0x04A // pop and release

#define INSTR_OBJ          0x04B // object for JSON 
#define INSTR_OBJ_END      0x04C // object end for JSON 
#define INSTR_BLOCK        0x04D // block 
#define INSTR_BLOCK_END    0x04E // block end 
#define INSTR_LOOP         0x04F // loop

#define INSTR_LOOP_END     0x050 // loop end
#define INSTR_TRY          0x051 // try
#define INSTR_TRY_END      0x052 // try end
#define INSTR_THROW        0x053 // throw
#define INSTR_CATCH        0x054 // catch
#define INSTR_INSTOF       0x055 // instanceof
#define INSTR_INCLUDE      0x056 // include
#define INSTR_STRICT       0x057 // strict 
#define INSTR_END          0x058 //END : end of code.
#define INSTR_ARR_SPREAD   0x059 // SPREAD_ARR : pop an array, append all elements to the array literal being built
#define INSTR_OBJ_SPREAD   0x05A // SPREAD_OBJ : pop an object, copy its members into the object literal being built
#define INSTR_CALL_SPREAD  0x05B // CALL_SPREAD x  : pop args array, call function x with runtime arity
#define INSTR_CALLO_SPREAD 0x05C // CALLO_SPREAD x : pop args array, call obj.x (obj beneath array) with runtime arity
#define INSTR_NEW_SPREAD   0x05D // NEW_SPREAD x   : pop args array, construct x with runtime arity
#define INSTR_MEMBERV      0x05E // MEMBERV : pop value, pop key, set scope-obj[key] = value (computed key)
#define INSTR_POW          0x05F // POW     : ** exponent
#define INSTR_POWEQ        0x060 // POWEQ   : **= exponent-assignment
#define INSTR_CALLX        0x061 // CALLX $n: call the function value sitting on the stack below its n args (IIFE / (expr)())
#define INSTR_CALLX_SPREAD 0x062 // CALLX_SPREAD : pop args array, call the function value beneath it with runtime arity
#define INSTR_TAG_RAW      0x063 // TAG_RAW : pop rawArr, pop stringsArr, set stringsArr.raw = rawArr, push stringsArr
#define INSTR_GETW         0x064 // GETW x  : member fetch as an assignment target (invokes a setter if the property is an accessor)
#define INSTR_YIELD        0x065 // YIELD   : pop the yielded value, suspend the generator; on resume push the value passed to next()
#define INSTR_YIELD_STAR   0x066 // YIELD*  : pop an iterable, delegate yields to it, push its return value
#define INSTR_FUNC_GEN     0x067 // FUNC_GEN: generator function/method definition (body follows like INSTR_FUNC)
#define INSTR_POS          0x068 // POS     : unary + (ToNumber of the value on the stack)
#define INSTR_OPT_GET      0x069 // OPT_GET x : optional-chaining member fetch `?.x`; nullish base -> undefined
#define INSTR_NULLISH      0x06A // NULLISH x : `??` short-circuit; if top is non-nullish jump x (keep it) else pop and fall through
#define INSTR_OREQ         0x06B // OREQ      : `||=` logical-OR assignment
#define INSTR_ANDEQ        0x06C // ANDEQ     : `&&=` logical-AND assignment
#define INSTR_NULLISHEQ    0x06D // NULLISHEQ : `??=` nullish assignment
#define INSTR_SET_PROTO    0x06E // SET_PROTO : pop v, set the object-under-construction's [[Prototype]] to v (`{__proto__: v}`)
#define INSTR_FUNC_ARROW   0x06F // FUNC_ARROW: define an ES6 arrow function (lexical this, no prototype, not constructible)
#define INSTR_GET_ITER     0x070 // GET_ITER  : pop an iterable, push its iterator (GetIterator: obj[Symbol.iterator]())
#define INSTR_ITER_STEP    0x071 // ITER_STEP : peek iterator, call next(); if done jump offset, else push step.value
#define INSTR_ARRAY_AT_M   0x072 // ARRAT_M : subscript that keeps the receiver (push receiver then member) for `obj[k](..)`
#define INSTR_CALLXO       0x073 // CALLXO $n: call the function value on the stack with the receiver beneath it (`obj[k](..)`)
#define INSTR_CALLXO_SPREAD 0x074 // CALLXO_SPREAD : pop args array, call the func value beneath it with the receiver beneath that (`obj[k](...a)`)
#define INSTR_INT64        0x075 // INT64 lo,hi   : push an int64 literal held in 2 consecutive PC words
#define INSTR_FLOAT64      0x076 // FLOAT64 lo,hi : push a double literal held in 2 consecutive PC words
#define INSTR_BIGINT       0x077 // BIGINT $n     : push a BigInt literal; the digit string rides the mstr_table like INSTR_STR (arbitrary width)
#define INSTR_DELETE       0x078 // DELETE $n     : pop obj, delete own member $n, push bool   (`delete o.x`)
#define INSTR_IN           0x079 // IN            : pop obj, pop key, push bool  (key in obj: own + prototype chain)
#define INSTR_DELETE_AT    0x07A // DELETE_AT     : pop key, pop obj, delete own member [key], push bool  (`delete o[k]`)
#define INSTR_DELETE_VAR   0x07B // DELETE_VAR $n : delete the global binding $n, push bool  (`delete x`)
#define INSTR_ARRAY_AT_W   0x07C // ARRAT_W : subscript as an assignment target; pops key + receiver, pushes a synthetic @@taslot node carrying the TypedArray + index (compiled from `ta[i] = ..` / `ta[i] += ..`)
#define INSTR_SWITCH       0x07D // SWITCH : push a switch scope (break anchor); pairs with INSTR_SWITCH_END
#define INSTR_SWITCH_END   0x07E // SWITCH_END : pop the switch scope
#define INSTR_BNOT         0x07F // BNOT : unary bitwise NOT `~x` (ToInt32 then invert; BigInt -> -(x+1))
#define INSTR_BITANDEQ     0x080 // BITANDEQ  : `&=`  bitwise-AND assignment (read-modify-write via the lvalue node)
#define INSTR_BITOREQ      0x081 // BITOREQ   : `|=`  bitwise-OR assignment
#define INSTR_BITXOREQ     0x082 // BITXOREQ  : `^=`  bitwise-XOR assignment
#define INSTR_LSHIFTEQ     0x083 // LSHIFTEQ  : `<<=` left-shift assignment
#define INSTR_RSHIFTEQ     0x084 // RSHIFTEQ  : `>>=` signed-right-shift assignment
#define INSTR_URSHIFTEQ    0x085 // URSHIFTEQ : `>>>=` unsigned-right-shift assignment
#define INSTR_SCOR         0x086 // SCOR  : short-circuit `||` (LHS truthy -> keep LHS, jump past RHS; else pop LHS, eval RHS)
#define INSTR_SCAND        0x087 // SCAND : short-circuit `&&` (LHS falsy  -> keep LHS, jump past RHS; else pop LHS, eval RHS)

#define INSTR_MAX          0x090 // Maximum instruction opcode value


PC          bc_gen(bytecode_t* bc, opr_code_t instr);
PC          bc_gen_str(bytecode_t* bc, opr_code_t instr, const char* s);
PC          bc_gen_int(bytecode_t* bc, opr_code_t instr, int32_t i);
PC          bc_gen_short(bytecode_t* bc, opr_code_t instr, int32_t i);
void        bc_set_instr(bytecode_t* bc, PC anchor, opr_code_t op, PC target);
void        bc_remove_instr(bytecode_t* bc, PC from, uint32_t num);
uint32_t    bc_getstrindex(bytecode_t* bc, const char* str);
PC          bc_add_instr(bytecode_t* bc, PC anchor, opr_code_t op, PC target);
PC          bc_reserve(bytecode_t* bc);

#define bc_getstr(bc, i) (((i)>=(bc)->mstr_table.size) ? "" : (const char*)(bc)->mstr_table.items[(i)])

void        bc_init(bytecode_t* bc);
void        bc_release(bytecode_t* bc);


/**====== mario_vm ======*/

extern const char* _mario_lang;

//script var
#define V_UNDEF  0
#define V_INT    1
#define V_FLOAT  2
#define V_STRING 3
#define V_OBJECT 4
#define V_BOOL   5
#define V_NULL   6
#define V_INT64  7  // int64_t   (exact large integers: 2^31..2^63-1)
#define V_FLOAT64 8  // double    (canonical float: every float literal / fractional result)
#define V_BIGINT 9  // bignum_t* (arbitrary-precision integer; typeof -> "bigint")

#define V_ST_FREE      0
#define V_ST_GC_FREE   1
#define V_ST_GC        2
#define V_ST_REF       3 

#define THIS "this"
#define PROTOTYPE "prototype"
#define SUPER "super"
#define CONSTRUCTOR "constructor"

/* ES6 Symbol: a symbol instance carries a hidden own member "@@symkey" whose
 * string value is the unique property-key the symbol maps to. All symbol keys
 * share the "@@S:" prefix so they never collide with internal bookkeeping
 * names or ordinary string keys, and getOwnPropertySymbols can find them. */
#define SYM_MARKER "@@symkey"
#define SYMKEY_PREFIX "@@S:"
#define SYMKEY_ITERATOR "@@S:iterator"
#define SYMKEY_ASYNCITERATOR "@@S:asyncIterator"
#define SYMKEY_TOSTRINGTAG "@@S:toStringTag"
#define SYMKEY_TOPRIMITIVE "@@S:toPrimitive"

/* Exotic objects (ArrayBuffer / SharedArrayBuffer / TypedArray / DataView /
 * Proxy) are ordinary V_OBJECT vars that carry ONE hidden invisable marker
 * member EXOTIC_MARKER whose string value is the kind, mirroring SYM_MARKER.
 * Plain objects never carry it, so var_is_exotic() is a single fast hash miss
 * and the property-access intercept added in Phases 3-5 is a genuine no-op for
 * them (behaviour-preserving). Raw bytes / trap tables ride sibling hidden
 * members; see the native_*.c constructors that set the marker. */
#define EXOTIC_MARKER "@@exotic"
#define EXOTIC_ARRAYBUFFER "ab"     // ArrayBuffer (raw byte holder)
#define EXOTIC_SHARED      "shared" // SharedArrayBuffer (ArrayBuffer + @@shared)
#define EXOTIC_TYPEDARRAY  "ta"     // TypedArray view over a buffer
#define EXOTIC_DATAVIEW    "dv"     // DataView over a buffer
#define EXOTIC_PROXY       "proxy"  // Proxy(target, handler)
#define EXOTIC_WEAKREF     "wr"     // WeakRef(target): weak observation via deref()
#define EXOTIC_FR          "fr"     // FinalizationRegistry: post-collection cleanup callbacks

/* TypedArray element-type codes (hidden @@etype member, a V_INT). One code per
 * concrete view; the element-access primitives in mario.c (var_typedarray_get_at
 * / _set_at) switch on it for width, signedness, clamping and BigInt storage.
 * TA_UINT8CLAMPED is the only non-wrapping integer type (round-half-even clamp). */
#define TA_INT8          0
#define TA_UINT8         1
#define TA_UINT8CLAMPED  2
#define TA_INT16         3
#define TA_UINT16        4
#define TA_INT32         5
#define TA_UINT32        6
#define TA_FLOAT32       7
#define TA_FLOAT64       8
#define TA_BIGINT64      9
#define TA_BIGUINT64     10
#define TA_ETYPE_COUNT   11

#define TA_ETYPE         "@@etype"   // hidden own member: the TA_* element-type code (V_INT)
#define TA_SLOT          "@@taslot"  // synthetic write-target node name (see INSTR_ARRAY_AT_W)
#define TA_SLOT_TA       "@@ta"      // hidden member on the @@taslot node's var: the ref'd TypedArray

/* Proxy exotic object (Phase 5): an ordinary V_OBJECT with @@exotic="proxy" plus
 * three hidden invisable members - @@ptarget (the target, ref'd), @@phandler (the
 * handler, ref'd) and @@prevoked (V_BOOL). The property-access intercept routes
 * get/set/has/deleteProperty/ownKeys/apply/construct through the handler's traps
 * (the proxy_* helpers in mario.c); a revoked proxy throws TypeError on every
 * operation. Extensibility is modelled by an @@noext marker (V_BOOL true) laid on
 * the target by preventExtensions - absent means extensible. */
#define PROXY_TARGET     "@@ptarget"   // hidden own member: the target object
#define PROXY_HANDLER    "@@phandler"  // hidden own member: the handler object
#define PROXY_REVOKED    "@@prevoked"  // hidden own member: V_BOOL, set by revoke()
#define PROXY_SLOT       "@@proxyslot" // synthetic write-target node name (proxy `set` intercept)
#define PROXY_SLOT_OBJ   "@@pobj"      // hidden member on the @@proxyslot node's var: the ref'd proxy
#define PROXY_SLOT_KEY   "@@pkey"      // hidden member on the @@proxyslot node's var: the key var
#define OBJ_NO_EXT       "@@noext"     // hidden own member (V_BOOL true): preventExtensions applied

/* WeakRef / FinalizationRegistry (Phase 6). A WeakRef holds its target's raw
 * pointer in var->value (NEVER ref'd, so the target stays collectable) with a
 * no-op free_func; value==NULL means the reference has been cleared. A
 * FinalizationRegistry keeps its cleanup callback as a hidden ref'd own member,
 * and each registration copies a reference into its C-side cell. */
#define FR_CALLBACK      "@@frcb"      // hidden own member: the ref'd cleanup callback

struct st_vm;
struct st_weak_cell;   // Phase 6 weak-reference / finalization cell (defined in mario.c)

typedef struct st_var {
	uint32_t            magic: 8; //0 for var; 1 for node
	uint32_t            type:10;
	uint32_t            status: 4;
	uint32_t            is_array:2;
	uint32_t            is_func:2;
	uint32_t            is_class:2;
	uint32_t            gc_marking: 2;
	uint32_t            gc_marked: 2;
	uint32_t            refs;

	uint32_t            size;  // size for bytes type of value;
	void*               value;

	free_func_t         free_func; //how to free value
	free_func_t         on_destroy; //before destroyed.

	struct st_var*      prev; //for var list
	struct st_var*      next; //for var list
	hash_map_t          children;
	struct st_vm*       vm;
} var_t;

typedef var_t* (*native_func_t)(struct st_vm *, var_t*, void*);
void        free_none(void* p);

typedef struct st_func {
	native_func_t       native;
	int8_t              regular: 4;
	int8_t              is_static: 4;
	int8_t              is_generator: 4; // ES6 `function*` / generator method
	int8_t              is_arrow: 4;     // ES6 arrow function: lexical `this`, no `prototype`, not constructible
	PC                  pc;
	void*               data;
	m_array_t           args; //argument names
	var_t*              owner;

	struct {
		var_t*             var;
		struct st_func*    func;
	} closure;
} func_t;

/* func_t.regular distinguishes a normal function/method from an ES6 accessor.
 * A getter and a setter share one property name, so the primary member var is
 * the getter when both exist (the setter hangs off it as the hidden member
 * FUNC_SETTER_KEY); otherwise the primary is whichever one was defined. */
#define FUNC_REGULAR   1
#define FUNC_GETTER    2
#define FUNC_SETTER    3
#define FUNC_SETTER_KEY "@@setter"

//script node for var member children
typedef struct st_node {
	uint32_t            magic: 8; //1 for node
	uint32_t            be_const : 8;
	uint32_t            be_inherited : 8;
	uint32_t            be_unenumerable : 4;
	uint32_t            invisable : 4;
	uint32_t            ncache_instr;
	char*               name;
	var_t*              var;
} node_t;

typedef struct st_ic_entry {
    m_array_t*          old_instr_pcs;     
    PC                  old_instr;      
    node_t*             node;
} load_ncache_t;

typedef bool (*compiler_func_t)(bytecode_t *bc, const char* input);

#define GC_TRIG_VAR_NUM_DEF 128
#define FREE_VAR_BUFFER_NUM_DEF 128

#define VAR_CACHE_MAX_DEF   128
#define LOAD_NCACHE_MAX_DEF 128

#define VM_STACK_MAX    32

//scope of vm runing
typedef struct st_scope {
	var_t* var;
	var_t* class_var; // for a class-definition scope: the constructor var (pushed by CLASS_END so class expressions evaluate to the class)
	PC pc_start; // continue anchor for loop
	PC pc; // try cache anchor , or break anchor for loop
	int32_t stack_top; // value-stack height at scope entry; a runtime throw truncates leaked operands back to the innermost func frame's height (see vm_throw_truncate)
	uint32_t is_func: 8;
	uint32_t is_block: 8;
	uint32_t is_try: 8;
	uint32_t is_loop: 4;
	uint32_t is_switch: 4; // switch scope: a `break` stops here, a `continue` does not (it belongs to an enclosing loop)
	uint32_t is_strict: 4;
	func_t*  func;
	struct st_scope* prev;
	//continue and break anchor for loop(while/for)
} scope_t;

#define VM_SCOPE_STACK_MAX    32

typedef struct st_vm {
	bytecode_t          bc;
	compiler_func_t     compiler;

	scope_t*            scope_stack[VM_SCOPE_STACK_MAX];
	int32_t             scope_stack_top;
	void*               stack[VM_STACK_MAX];
	int32_t             stack_top;
	PC                  pc;

	bool                terminated;
	/* Instruction-level service hook: vm_run() calls on_step every
	 * step_interval dispatched instructions (0 = disabled; vm_new zeroes it).
	 * The embedder uses it as a page-independent cadence to pump UI events and
	 * enforce a wall-clock run budget: set terminated=true inside the hook to
	 * unwind every nested vm_run frame, then call vm_terminate() and clear
	 * terminated once the run has fully returned. The hook must not compile or
	 * load code (the bytecode buffer may not grow mid-run). */
	uint32_t            step_interval;
	uint32_t            step_count;
	void                (*on_step)(struct st_vm* vm, void* data);
	void*               on_step_data;
	/* ES6 generator suspension: handle_yield sets yielded + yield_value and the
	 * running vm_run() returns; gen_resume() (the generator's next()) consumes
	 * them. yield_delegate carries the iterator of an in-progress `yield*`.
	 * gen_depth>0 disables the LOAD inline cache while a generator frame runs
	 * (suspension extends node lifetimes across resumes -> stale cache). */
	bool                yielded;
	var_t*              yield_value;
	var_t*              yield_delegate;
	/* An exception raised inside a native function (vm_throw_native): func_call
	 * delivers it to the nearest try scope after the native returns, keeping the
	 * value stack balanced (env pop / ret push protocol). */
	var_t*              native_thrown;
	uint32_t            gen_depth;
	var_t*              root;
	var_t*              new_target; // ES6 `new.target`: the constructor of the in-progress `new`; consumed (bound into env, then cleared) by func_call
	int32_t             to_str_depth; // guards var_to_str's object->toString() call against unbounded re-entrancy

	m_array_t           included;

	void                (*on_init)(struct st_vm* vm);
	m_array_t           init_natives;
	void                (*on_close)(struct st_vm* vm);
	m_array_t           close_natives;

	struct {
		var_t**         cache;
		uint32_t        size;
		uint32_t        used;
	} var_cache;

	struct {
		load_ncache_t*  cache;
		uint32_t        size;
	} load_ncache;

	uint32_t            this_strIndex;
	struct {
		var_t*          var_Object;
		var_t*          var_String;
		var_t*          var_Number;
		var_t*          var_BigInt;
		var_t*          var_Error;
		var_t*          var_Array;
		var_t*          var_true;
		var_t*          var_false;
		var_t*          var_null;
	} builtin_vars;

	// GC structure
	struct {
		bool            is_doing_gc;
		uint32_t        gc_defer; //>0 while the object graph is temporarily unrooted (var_clean teardown / func_call arg setup); defer opportunistic gc.
		uint32_t        gc_trig_var_num; //trigger gc when var num reach this value.
		uint32_t        free_var_buffer_num; // number of free var buffer.
		var_t*          gc_vars;
		var_t*          gc_vars_tail;
		uint32_t        gc_vars_num;
	} gc;

	/* Phase 6: weak references & finalization. Cells live in these C-side lists,
	 * NOT the var graph: var_clean() consults weak_cells when a target dies (clear
	 * WeakRefs, queue finalizers) and gc_mark_weak() shields the held value +
	 * callback a pending finalizer still needs. All zero for a program that never
	 * uses WeakRef/FinalizationRegistry, so the common path costs one pointer test. */
	struct st_weak_cell* weak_cells;
	struct st_weak_cell* pending_finalizers;
	bool                 finalizers_draining; // re-entrancy guard for vm_drain_finalizers

	var_t*              free_var_buffer;
	uint32_t            free_var_buffer_num;
} vm_t;

typedef mstr_t* (*load_m_func_t)(struct st_vm *, const char* jsname);
extern load_m_func_t _load_m_func;

node_t*     node_new(vm_t* vm, const char* name, var_t* var);
void        node_free(void* p);
var_t*      node_replace(node_t* node, var_t* v);

void        var_remove_all(var_t* var);
node_t*     var_add(var_t* var, const char* name, var_t* add);
node_t*     var_add_head(var_t* var, const char* name, var_t* add);
node_t*     var_find_own_member(var_t* var, const char*name);
var_t*      var_find_own_member_var(var_t* var, const char*name);
node_t*     var_find_member_create(var_t* var, const char*name);
node_t*     var_get(var_t* var, int32_t index);

node_t*     var_array_get(var_t* var, int32_t index);
var_t*      var_array_get_var(var_t* var, int32_t index);
node_t*     var_array_add(var_t* var, var_t* add);
node_t*     var_array_add_head(var_t* var, var_t* add);
node_t*     var_array_set(var_t* var, int32_t index, var_t* set_var);
node_t*     var_array_remove(var_t* var, int32_t index);
void        var_array_del(var_t* var, int32_t index);
void        var_array_reverse(var_t* var);
uint32_t    var_array_size(var_t* var);
void        var_instance_from(var_t* var, var_t* src);
void        var_clean(var_t* var);

var_t*      var_ref(var_t* var);
void        var_unref(var_t* var);

//#define var_ref(var) ({ ++(var)->refs; var; })
//#define var_unref(var, del) ({ --(var)->refs; if((var)->refs <= 0 && (del)) var_free((var)); })

var_t*      var_new(vm_t* vm);
var_t*      var_new_block(vm_t* vm);
var_t*      var_new_array(vm_t* vm);
var_t*      var_new_int(vm_t* vm, int i);
var_t*      var_new_null(vm_t* vm);
var_t*      var_new_bool(vm_t* vm, bool b);
var_t*      var_new_obj_no_proto(vm_t* vm, void*p, free_func_t fr);
var_t*      var_new_obj(vm_t* vm, var_t* proto, void*p, free_func_t fr);
var_t*      var_new_float(vm_t* vm, float i);
var_t*      var_new_str(vm_t* vm, const char* s);
var_t*      var_new_str2(vm_t* vm, const char* s, uint32_t len);
const char* var_get_str(var_t* var);
var_t*      var_set_str(var_t* var, const char* v);
int         var_get_int(var_t* var);
var_t*      var_set_int(var_t* var, int v);
bool        var_get_bool(var_t* var);
float       var_get_float(var_t* var);
var_t*      var_set_float(var_t* var, float v);
var_t*      var_new_int64(vm_t* vm, int64_t i);
int64_t     var_get_int64(var_t* var);
var_t*      var_set_int64(var_t* var, int64_t v);
var_t*      var_new_float64(vm_t* vm, double d);
double      var_get_float64(var_t* var);
var_t*      var_set_float64(var_t* var, double v);
var_t*      var_new_bigint(vm_t* vm, bignum_t* b); // takes ownership of b (freed via bn_free)
bignum_t*   var_get_bigint(var_t* var);            // NULL unless var is a V_BIGINT
bool        var_is_number(var_t* var);
func_t*     var_get_func(var_t* var);
var_t*      var_get_prototype(var_t* var);
void        var_set_prototype(var_t* var, var_t* proto);
bool        var_instanceof(var_t* var, var_t* proto);
node_t*     var_find_member(var_t* obj, const char* name);
var_t*      var_find_member_var(var_t* obj, const char* name);
var_t*      var_find_own_member_var(var_t* obj, const char* name);

void        var_to_json_str(var_t*, mstr_t*, int, bool);
void        var_to_str(var_t*, mstr_t*);

bool        var_is_symbol(var_t* var);
const char* var_symbol_key(var_t* var);

/* Exotic-object markers (see EXOTIC_MARKER). var_is_exotic is the single cheap
 * discriminator the property-access intercept gates on; the kind predicates read
 * the marker's string value. All are false for plain objects. */
bool        var_is_exotic(var_t* var);
const char* var_exotic_kind(var_t* var);      // marker value ("ab"/"ta"/...) or NULL
bool        var_is_arraybuffer(var_t* var);   // ArrayBuffer or SharedArrayBuffer
bool        var_is_typedarray(var_t* var);
bool        var_is_dataview(var_t* var);
bool        var_is_proxy(var_t* var);

/* TypedArray element access, implemented in mario.c next to the exotic
 * predicates so the read/write intercept does not depend on the lang native.
 * get_at decodes element `idx` of the buffer into a fresh var (OOB -> NULL,
 * which the caller turns into undefined); set_at encodes `val` into element
 * `idx` (clamping / wrapping / BigInt per the etype) and returns false on OOB
 * or a detached/mis-typed receiver. Both are host-endian and memcpy-safe. */
var_t*      var_typedarray_get_at(vm_t* vm, var_t* ta, int64_t idx);
bool        var_typedarray_set_at(vm_t* vm, var_t* ta, int64_t idx, var_t* val);

/* Member removal / presence used by the `delete` and `in` operators (and by
 * Reflect.deleteProperty / Reflect.has in Phase 5). */
bool        var_delete_own_member(var_t* obj, const char* name);
bool        var_has_member(var_t* obj, const char* name); // own + prototype chain

/* Proxy trap plumbing (Phase 5), implemented in mario.c next to the exotic
 * predicates so the property-access intercept does not depend on the lang native.
 * var_is_callable is true for a function OR a proxy whose target is callable (so
 * the call/new paths can route an apply/construct trap). The proxy_* entry points
 * invoke the matching handler trap and fall back to the default operation on the
 * target when the trap is absent; get/own_keys/apply/construct return an OWNED var
 * (the caller pushes-then-unrefs it, or normalises it for a native return), the
 * rest return a bool result. mario_*_var are the proxy-aware read/write/presence/
 * delete/ownKeys primitives shared by the VM intercept and Reflect.* - for a
 * non-proxy receiver they are exactly the ordinary member operations. */
bool        var_is_callable(var_t* v);
var_t*      var_proxy_target(var_t* p);
var_t*      var_proxy_handler(var_t* p);
bool        var_proxy_is_revoked(var_t* p);
var_t*      mario_get_var(vm_t* vm, var_t* obj, var_t* key, var_t* receiver);      // owned
bool        mario_set_var(vm_t* vm, var_t* obj, var_t* key, var_t* value, var_t* receiver);
bool        mario_has_var(vm_t* vm, var_t* obj, var_t* key);
bool        mario_delete_var(vm_t* vm, var_t* obj, var_t* key);
var_t*      mario_own_keys_var(vm_t* vm, var_t* obj, bool strings_only, bool enum_only); // owned array
var_t*      proxy_get(vm_t* vm, var_t* p, var_t* key, var_t* receiver);            // owned
bool        proxy_set(vm_t* vm, var_t* p, var_t* key, var_t* value, var_t* receiver);
bool        proxy_has(vm_t* vm, var_t* p, var_t* key);
bool        proxy_delete(vm_t* vm, var_t* p, var_t* key);
var_t*      proxy_own_keys(vm_t* vm, var_t* p, bool strings_only, bool enum_only); // owned array
var_t*      proxy_apply(vm_t* vm, var_t* p, var_t* thisArg, var_t* args);          // owned
var_t*      proxy_construct(vm_t* vm, var_t* p, var_t* args, var_t* newTarget);    // owned
var_t*      proxy_get_prototype(vm_t* vm, var_t* p);                               // owned or NULL
bool        proxy_set_prototype(vm_t* vm, var_t* p, var_t* proto);
bool        proxy_is_extensible(vm_t* vm, var_t* p);
bool        proxy_prevent_extensions(vm_t* vm, var_t* p);
bool        proxy_define_property(vm_t* vm, var_t* p, var_t* key, var_t* desc);
var_t*      proxy_get_own_descriptor(vm_t* vm, var_t* p, var_t* key);             // owned or NULL

/* Proxy-aware primitives for the remaining internal methods (Reflect.* and the
 * Object.* statics): a proxy routes its trap, any other object the default op on
 * itself. var-returning ops yield a baseline (refs==0) value or a borrowed
 * persistent prototype (getPrototypeOf); bool ops yield the operation result. */
var_t*      mario_get_prototype_var(vm_t* vm, var_t* obj);        // owned or borrowed or NULL
bool        mario_set_prototype_var(vm_t* vm, var_t* obj, var_t* proto);
bool        mario_is_extensible_var(vm_t* vm, var_t* obj);
bool        mario_prevent_extensions_var(vm_t* vm, var_t* obj);
bool        mario_define_property_var(vm_t* vm, var_t* obj, var_t* key, var_t* desc);
var_t*      mario_gopd_var(vm_t* vm, var_t* obj, var_t* key);     // owned or undefined
var_t*      mario_apply_var(vm_t* vm, var_t* func, var_t* thisArg, var_t* argsNatural);   // owned
var_t*      mario_construct_var(vm_t* vm, var_t* ctor, var_t* argsNatural, var_t* newTarget); // owned

var_t*      vm_get_iterator(vm_t* vm, var_t* iterable);
var_t*      vm_new_array_iterator(vm_t* vm, var_t* arr);
var_t*      vm_new_string_iterator(vm_t* vm, var_t* str);

void        vm_push(vm_t* vm, var_t* var);
void        vm_push_node(vm_t* vm, node_t* node);
bool        vm_pop(vm_t* vm);
var_t*      vm_pop2(vm_t* vm);

vm_t*       vm_new(compiler_func_t compiler, uint32_t var_cache_size, uint32_t load_ncache_size);
node_t*     vm_load_node(vm_t* vm, const char* name, bool create);

void        vm_init(vm_t* vm,
		void (*on_init)(struct st_vm* vm),
		void (*on_close)(struct st_vm* vm)
);

vm_t*       vm_from(vm_t* vm);

bool        vm_load(vm_t* vm, const char* s);
bool        vm_load_run(vm_t* vm, const char* s);
bool        vm_load_run_native(vm_t* vm, const char* s);
bool        vm_run(vm_t* vm);
void        vm_close(vm_t* vm);
void        vm_terminate(vm_t* vm);

var_t*      vm_new_class(vm_t* vm, const char* cls);
var_t*      new_obj(vm_t* vm, const char* cls_name, int arg_num);
void        vm_throw(vm_t* vm, const char* format, ...);
void        vm_throw_native(vm_t* vm, const char* format, ...);
/* Throw a typed error (e.g. "TypeError"/"RangeError"): builds an instance whose
 * [[Prototype]] is that class's prototype (so `instanceof` and `.name` work),
 * then unwinds to the nearest try scope like vm_throw. */
void        vm_throw_type(vm_t* vm, const char* type_name, const char* format, ...);
/* Deferred typed throw for use INSIDE a native: records the typed error in
 * vm->native_thrown (like vm_throw_native) so func_call unwinds after the native
 * returns and the value stack stays balanced. The native still returns a dummy. */
void        vm_throw_type_native(vm_t* vm, const char* type_name, const char* format, ...);
node_t*     vm_find(vm_t* vm, const char* name);
node_t*     vm_find_in_class(var_t* var, const char* name);
node_t*     vm_reg_var(vm_t* vm, var_t* cls, const char* name, var_t* var, bool be_const);
node_t*     vm_reg_static(vm_t* vm, var_t* cls, const char* decl, native_func_t native, void* data);
node_t*     vm_reg_native(vm_t* vm, var_t* cls, const char* decl, native_func_t native, void* data);
node_t*     vm_reg_native_on(vm_t* vm, var_t* target, const char* decl, native_func_t native, void* data);
void        vm_mark_func_scopes(vm_t* vm, var_t* func);
void        vm_reg_init(vm_t* vm, void (*func)(void*), void* data);
void        vm_reg_close(vm_t* vm, void (*func)(void*), void* data);

/* Phase 6: weak references & finalization registry (defined in mario.c). A
 * WeakRef observes a target without keeping it alive; a FinalizationRegistry
 * queues a cleanup callback for when a target is collected. The natives in
 * native_WeakRef.c / native_FinalizationRegistry.c drive these; var_clean() and
 * gc_vars() consult the registry internally. */
void        vm_weak_add_ref(vm_t* vm, var_t* target, var_t* weakref);
void        vm_weak_remove_ref(vm_t* vm, var_t* weakref);
void        vm_weak_add_finalizer(vm_t* vm, var_t* registry, var_t* target, var_t* callback, var_t* held, var_t* token);
bool        vm_weak_unregister(vm_t* vm, var_t* registry, var_t* token);
void        vm_weak_remove_registry(vm_t* vm, var_t* registry);
void        vm_gc_collect(vm_t* vm); // forced full gc + drain pending finalizers (backs the hidden gc() global)

var_t*      get_obj(var_t* obj, const char* name);
void*       get_raw(var_t* obj, const char* name);
const char* get_str(var_t* obj, const char* name);
int         get_int(var_t* obj, const char* name);
float       get_float(var_t* obj, const char* name);
double      get_float64(var_t* obj, const char* name);
bool        get_bool(var_t* obj, const char* name);
var_t*      get_obj_member(var_t* obj, const char* name);
var_t*      set_obj_member(var_t* obj, const char* name, var_t* var);

var_t*      get_func_args(var_t* env);
uint32_t    get_func_args_num(var_t* env);
var_t*      get_func_arg(var_t* env, uint32_t index);
int         get_func_arg_int(var_t* env, uint32_t index);
bool        get_func_arg_bool(var_t* env, uint32_t index);
float       get_func_arg_float(var_t* env, uint32_t index);
const char* get_func_arg_str(var_t* env, uint32_t index);
var_t*      call_m_func(vm_t* vm, var_t* obj, var_t* func, var_t* args);
var_t*      call_m_func_by_name(vm_t* vm, var_t* obj, const char* func_name, uint32_t arg_num, ... );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
