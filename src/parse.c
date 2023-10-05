#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdalign.h>
#include "ejson.h"

static bool is_space(char c)
{
    return c == ' ' || c == '\t'
        || c == '\r' || c == '\n';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

static bool is_printable(char c)
{
    return c >= 32 && c < 127;
}

static void report(ejson_error *error, const char *fmt, ...)
{
    if (!error)
        return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error->msg, sizeof(error->msg), fmt, args);
    va_end(args);
}

ejson_value *ejson_seekbykey2(ejson_value *value, const char *key, size_t size)
{
    if (value->type != EJSON_OBJECT)
        return NULL;

    for (ejson_iter iter = ejson_iterover(value); ejson_next(&iter); ) {
        ejson_string iterkey = iter.val->key;
        if (iterkey.size == size && !strncmp(iterkey.base, key, size))
            return iter.val;
    }

    return NULL;
}

ejson_value *ejson_seekbykey(ejson_value *value, const char *key)
{
    return ejson_seekbykey2(value, key, strlen(key));
}

ejson_value *ejson_seekbyindex(ejson_value *value, size_t index)
{
    for (ejson_iter iter = ejson_iterover(value); ejson_next(&iter); )
        if (index == iter.idx)
            return iter.val;
    return NULL;
}

typedef struct {
    ejson_error *error;
    ejson_arena *arena;
    const char *src;
    size_t cur, len;
    ejson_config config;
} context_t;

static bool follows_space(context_t *ctx)
{
    return ctx->cur < ctx->len && is_space(ctx->src[ctx->cur]);
}

static void consume_spaces(context_t *ctx)
{
    while (follows_space(ctx))
        ctx->cur++;
}

static bool parse_str(context_t *ctx, ejson_string *str)
{
    assert(str);
    assert(ctx->cur < ctx->len);
    
    char first = ctx->src[ctx->cur];
    assert(first == '\'' || first == '"');

    ctx->cur++; // Consume the double quotes

    size_t off = ctx->cur;
    while (ctx->cur < ctx->len && ctx->src[ctx->cur] != first)
        ctx->cur++;
    size_t len = ctx->cur - off;

    if (ctx->cur == ctx->len) {
        report(ctx->error, "No closing %s after string", first == '"' ? "'\"'" : "'\\''");
        return false;
    }
    ctx->cur++; // Consume the "\"" or "'"

    str->base = ctx->src + off;
    str->size = len;
    return true;
}

static void *alloc(ejson_arena *arena, size_t size, size_t align)
{
    size_t pad = -arena->used & (align-1);
    arena->used += pad;
    
    if (arena->used + size > arena->size)
        return NULL;

    void *p = arena->base + arena->used;
    arena->used += size;

    return p;
}

static void *alloc_or_report(context_t *ctx, size_t size, size_t align)
{
    void *mem = alloc(ctx->arena, size, align);
    if (mem == NULL) {
        report(ctx->error, "Out of arena");
        return NULL;
    }
    return mem;
}

#define EMPTY_STRING ((ejson_string) {.base=NULL, .size=0})

static void init_val_for_str(ejson_value *val, ejson_string str)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_STRING;
    val->key  = EMPTY_STRING;
    val->when_string = str;
}

static void init_val_for_obj(ejson_value *val, ejson_value *head, size_t size)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_OBJECT;
    val->key  = EMPTY_STRING;
    val->when_array.head = head;
    val->when_array.size = size;
}

static void init_val_for_arr(ejson_value *val, ejson_value *head, size_t size)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_ARRAY;
    val->key  = EMPTY_STRING;
    val->when_array.head = head;
    val->when_array.size = size;
}

static void init_val_for_int(ejson_value *val, int64_t raw)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_NUMBER;
    val->key  = EMPTY_STRING;
    val->when_number.as_int = raw;
    val->when_number.as_flt = raw;
}

static void init_val_for_flt(ejson_value *val, double raw)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_NUMBER;
    val->key  = EMPTY_STRING;
    val->when_number.as_int = raw;
    val->when_number.as_flt = raw;
}

static void init_val_for_null(ejson_value *val)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_NULL;
    val->key  = EMPTY_STRING;
}

static void init_val_for_true(ejson_value *val)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_BOOLEAN;
    val->key  = EMPTY_STRING;
    val->when_boolean = 1;
}

static void init_val_for_false(ejson_value *val)
{
    val->prev = NULL;
    val->next = NULL;
    val->type = EJSON_BOOLEAN;
    val->key  = EMPTY_STRING;
    val->when_boolean = 0;
}

static ejson_value *make_val_for_str(context_t *ctx, ejson_string str)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_str(val, str);
    return val;
}

static ejson_value *make_val_for_obj(context_t *ctx, ejson_value *head, size_t size)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_obj(val, head, size);
    return val;
}

static ejson_value *make_val_for_arr(context_t *ctx, ejson_value *head, size_t size)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_arr(val, head, size);
    return val;
}

static ejson_value *make_val_for_int(context_t *ctx, int64_t raw)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_int(val, raw);
    return val;
}

static ejson_value *make_val_for_flt(context_t *ctx, double raw)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_flt(val, raw);
    return val;
}

static ejson_value *make_val_for_null(context_t *ctx)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_null(val);
    return val;
}

static ejson_value *make_val_for_true(context_t *ctx)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_true(val);
    return val;
}

static ejson_value *make_val_for_false(context_t *ctx)
{
    ejson_value *val = alloc_or_report(ctx, sizeof(ejson_value), alignof(ejson_value));
    if (val) init_val_for_false(val);
    return val;
}

static ejson_value *make_val_for_empty_obj(context_t *ctx)
{
    return make_val_for_obj(ctx, NULL, 0);
}

static ejson_value *make_val_for_empty_arr(context_t *ctx)
{
    return make_val_for_arr(ctx, NULL, 0);
}

static ejson_value *parse_str_2(context_t *ctx)
{
    ejson_value *val;
    ejson_string str;
    if (!parse_str(ctx, &str))
        val = NULL;
    else
        val = make_val_for_str(ctx, str);
    return val;
}

static ejson_value *parse_any(context_t *ctx);

static ejson_value *parse_obj(context_t *ctx)
{
    assert(ctx->cur < ctx->len && ctx->src[ctx->cur] == '{');

    ctx->cur++; // Consume the "{"

    // Check wether the object has no items
    consume_spaces(ctx);
    if (ctx->cur == ctx->len) {
        report(ctx->error, "Source end in object");
        return NULL;
    }
    if (ctx->src[ctx->cur] == '}') {
        ctx->cur++; // Consume the "}"
        return make_val_for_empty_obj(ctx);
    }

    // At this point the cursor refers to the key
    // of the first element.

    ejson_value  *head;
    ejson_value **tail = &head;
    size_t size = 0;
    do {
        char c;

        // Make sure a string value follors

        assert(ctx->cur < ctx->len);

        c = ctx->src[ctx->cur];
        if (c != '"') {
            if (is_printable(c))
                report(ctx->error, "Missing key (character '%c' instead)", c);
            else
                report(ctx->error, "Invalid byte %x in object", c);
            return NULL;
        }
        
        // Parse the key string

        ejson_string key;
        if (!parse_str(ctx, &key))
            return NULL;

        // Consume the key-value separator ':'
        consume_spaces(ctx);
        if (ctx->cur == ctx->len) {
            report(ctx->error, "Source end in object (after key)");
            return NULL;
        }
        c = ctx->src[ctx->cur];
        if (c != ':') {
            if (is_printable(c))
                report(ctx->error, "Missing ':' after key (character '%c' instead)", c);
            else
                report(ctx->error, "Invalid byte %x in object (after key)", c);
            return NULL;
        }
        ctx->cur++; // Consume the ":"

        ejson_value *val = parse_any(ctx);
        if (!val)
            return NULL;

        // Insert the value into the object
        val->key = key;
        val->prev = tail;
        *tail = val;
        tail = &val->next;
        size++;

        // Now prepare for the next element
        consume_spaces(ctx);
        if (ctx->cur == ctx->len) {
            report(ctx->error, "Source end in object (after value)");
            return NULL;
        }
        c = ctx->src[ctx->cur];
        if (c == '}') {
            ctx->cur++;
            break;
        }
        if (c != ',') {
            if (is_printable(c))
                report(ctx->error, "Missing ',' or '}' after value (character '%c' instead)", c);
            else
                report(ctx->error, "Invalid byte %x in object (after value)", c);
            return NULL;
        }
        ctx->cur++; // Consume the ","

        consume_spaces(ctx);
        if (ctx->cur == ctx->len) {
            report(ctx->error, "Source end in object (after '%c')", c);
            return NULL;
        }

    } while (1);

    *tail = NULL;

    return make_val_for_obj(ctx, head, size);
}

static ejson_value *parse_arr(context_t *ctx)
{
    assert(ctx->cur < ctx->len && ctx->src[ctx->cur] == '[');

    ctx->cur++; // Consume the "["
    
    // Check wether the array has no items
    consume_spaces(ctx);
    if (ctx->cur == ctx->len) {
        report(ctx->error, "Source end in array");
        return NULL;
    }
    if (ctx->src[ctx->cur] == ']') {
        ctx->cur++; // Consume the "]"
        return make_val_for_empty_arr(ctx);
    }

    ejson_value  *head;
    ejson_value **tail = &head;
    size_t size = 0;

    for (;;) {
        ejson_value *val = parse_any(ctx);
        if (!val)
            return NULL;

        // Insert the value into the array
        val->prev = tail;
        *tail = val;
        tail = &val->next;
        size++;

        // Now prepare for the next element
        consume_spaces(ctx);
        if (ctx->cur == ctx->len) {
            report(ctx->error, "Source end in array (after value)");
            return NULL;
        }
        char c = ctx->src[ctx->cur];
        if (c == ']') {
            ctx->cur++;
            break;
        }
        if (c != ',') {
            if (is_printable(c))
                report(ctx->error, "Missing ',' or ']' after value (character '%c' instead)", c);
            else
                report(ctx->error, "Invalid byte %x in array (after value)", c);
            return NULL;
        }
        ctx->cur++; // Consume the ","

        consume_spaces(ctx);
        if (ctx->cur == ctx->len) {
            report(ctx->error, "Source end in array (after '%c')", c);
            return NULL;
        }
    }

    *tail = NULL;
    return make_val_for_arr(ctx, head, size);
}

static bool follows_digit(context_t *ctx)
{
    return ctx->cur < ctx->len && is_digit(ctx->src[ctx->cur]);
}

static ejson_value *parse_int(context_t *ctx)
{
    assert(follows_digit(ctx));

    int64_t value = 0;
    do {
        char c = ctx->src[ctx->cur]; // Digit character
        int  d = c & 0x0F; // Integer value of the character
        if (value > (INT64_MAX - d) / 10) {
            report(ctx->error, "Overflow");
            return NULL;
        }
        value = value * 10 + d;
        ctx->cur++;
    } while (follows_digit(ctx));

    return make_val_for_int(ctx, value);
}

static ejson_value *parse_flt(context_t *ctx)
{
    assert(follows_digit(ctx));

    double value = 0;
    do {
        char c = ctx->src[ctx->cur]; // Digit character
        int  d = c & 0x0F; // Integer value of the character
        value = value * 10 + d;
        ctx->cur++;
    } while (follows_digit(ctx));

    assert(ctx->cur < ctx->len && ctx->src[ctx->cur] == '.');
    ctx->cur++;

    if (follows_digit(ctx)) {

        double q = 1;
        do {
            q /= 10;
            char c = ctx->src[ctx->cur];
            int  d = c & 0x0F;
            value += q * d;
            ctx->cur++;
        } while (follows_digit(ctx));
    }

    return make_val_for_flt(ctx, value);
}

static ejson_value *parse_num(context_t *ctx)
{
    size_t cur = ctx->cur;
    while (cur < ctx->len && is_digit(ctx->src[cur]))
        cur++;

    if (cur < ctx->len && ctx->src[cur] == '.')
        return parse_flt(ctx);
    return parse_int(ctx);
}

static bool follows_alpha(context_t *ctx)
{
    return ctx->cur < ctx->len && is_alpha(ctx->src[ctx->cur]);
}

static ejson_value *parse_oth(context_t *ctx)
{
    assert(ctx->cur < ctx->len);

    char c = ctx->src[ctx->cur];
    if (!is_alpha(c)) {
        if (is_printable(c))
            report(ctx->error, "Unexpected character '%c'", c);
        else
            report(ctx->error, "Invalid byte %x", c);
        return NULL;
    }

    size_t off = ctx->cur;
    do
        ctx->cur++;
    while (follows_alpha(ctx));
    size_t len = ctx->cur - off;

    assert(len > 0);

    if (len == 4 && !strncmp("null", ctx->src + off, 4))
        return make_val_for_null(ctx);

    if (len == 4 && !strncmp("true", ctx->src + off, 4))
        return make_val_for_true(ctx);

    if (len == 5 && !strncmp("false", ctx->src + off, 5))
        return make_val_for_false(ctx);

    report(ctx->error, "Invalid token '%.*s'", (int) len, ctx->src + off);
    return NULL;
}

static ejson_value *parse_any(context_t *ctx)
{
    consume_spaces(ctx);

    if (ctx->cur == ctx->len) {
        report(ctx->error, "Missing value");
        return NULL;
    }

    char c = ctx->src[ctx->cur];

    if (c == '"' || (c == '\'' && ctx->config.allow_single_quoted_strings))
        return parse_str_2(ctx);
    
    if (c == '{')
        return parse_obj(ctx);
    
    if (c == '[')
        return parse_arr(ctx);
    
    if (is_digit(c))
        return parse_num(ctx);

    return parse_oth(ctx);
}

ejson_value *ejson_parse2(const char *src, size_t len, size_t *end,
                          ejson_error *error, ejson_arena *arena,
                          ejson_config config)
{
    size_t save = arena->used;

    context_t ctx = {
        .error = error,
        .arena = arena,
        .src = src,
        .len = len,
        .cur = 0,
        .config = config,
    };

    ejson_value *root = parse_any(&ctx);
    if (root == NULL)
        arena->used = save;
    else {
        if (end) 
            *end = ctx.cur;
    }
    return root;
}

ejson_value *ejson_parse(const char *src, size_t len,
                         ejson_error *error, ejson_arena *arena)
{
    return ejson_parse2(src, len, NULL, error, arena, EJSON_DEFAULT_CONFIGS);
}
