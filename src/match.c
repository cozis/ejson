#include <string.h>
#include <assert.h>
#include "ejson.h"

typedef struct {
    const char *fmt;
    size_t cur, len;
    ejson_value **out;
    size_t nout;
} context_t;

static bool is_space(char c)
{
    return c == ' '  || c == '\t' 
        || c == '\r' || c == '\n';
}

static void consume_spaces(context_t *ctx)
{
    while (ctx->cur < ctx->len && is_space(ctx->fmt[ctx->cur]))
        ctx->cur++;
}

static ejson_matchresult match_and_unpack(context_t *ctx, ejson_value *val);

static ejson_matchresult match_and_unpack_arr(context_t *ctx, ejson_value *val)
{
    assert(val->type == EJSON_ARRAY);

    if (ctx->cur == ctx->len || ctx->fmt[ctx->cur] != '[')
        return EJSON_NOMATCH;
    ctx->cur++; // Consume the "["

    // TODO: Handle empty array in fmt

    for (ejson_iter iter = ejson_iterover(val); ejson_next(&iter); ) {

        consume_spaces(ctx);
        if (ctx->cur == ctx->len)
            return EJSON_BADFORMAT;

        ejson_matchresult res = match_and_unpack(ctx, iter.val);
        if (res) 
            return res;

        consume_spaces(ctx);
        if (ctx->cur == ctx->len)
            return EJSON_BADFORMAT;

        if (ejson_hasnext(iter.val)) {
            if (ctx->fmt[ctx->cur] == ']') {
                ctx->cur++;
                break;
            }
            if (ctx->fmt[ctx->cur] != ',') 
                return EJSON_BADFORMAT;
        } else {
            if (ctx->fmt[ctx->cur] == ',') 
                return EJSON_NOMATCH;
            if (ctx->fmt[ctx->cur] != ']') 
                return EJSON_BADFORMAT;
        }
        ctx->cur++; // Consume the "," or "]"
    }
    
    return EJSON_MATCH;
}

static ejson_value *parse_next_value_in_fmt(context_t *ctx, char *mem, size_t max)
{
    ejson_arena arena = {
        .base=mem, 
        .size=max, 
        .used=0,
    };

    ejson_config config = EJSON_DEFAULT_CONFIGS;
    config.allow_single_quoted_strings = true;

    const char *substr = ctx->fmt + ctx->cur;
    size_t      sublen = ctx->len - ctx->cur;
    
    size_t num;    
    ejson_value *key = ejson_parse2(substr, sublen, &num, NULL, &arena, config);
    if (key) ctx->cur += num;
    
    return key;
}

static ejson_matchresult match_and_unpack_obj(context_t *ctx, ejson_value *val)
{
    assert(val->type == EJSON_OBJECT);

    if (ctx->cur == ctx->len || ctx->fmt[ctx->cur] != '{')
        return EJSON_NOMATCH;
    ctx->cur++; // Consume the "{"

    // TODO: Handle empty object in fmt

    while (1) {

        consume_spaces(ctx);
        if (ctx->cur == ctx->len)
            return EJSON_BADFORMAT;

        char pool[1024];
        ejson_value *key = parse_next_value_in_fmt(ctx, pool, sizeof(pool));
        if (key == NULL || key->type != EJSON_STRING)
            // Either the format contains invalid JSON at this point
            // or it contains a value other than a string.
            return EJSON_BADFORMAT;

        // Now find the value in the object with the given key
        ejson_value *child = ejson_seekbykey2(val, key->when_string.base, key->when_string.size);
        if (child == NULL)
            // Item specified in the format isn't contained 
            // by the source object.
            return EJSON_NOMATCH;

        // Now expect a ':' in the format
        consume_spaces(ctx);
        if (ctx->cur == ctx->len || ctx->fmt[ctx->cur] != ':')
            return EJSON_BADFORMAT;
        ctx->cur++; // Consume the ":"

        ejson_matchresult res = match_and_unpack(ctx, child);
        if (res) 
            return res;

        consume_spaces(ctx);
        if (ctx->cur == ctx->len)
            return EJSON_BADFORMAT;

        if (ctx->fmt[ctx->cur] == '}') {
            ctx->cur++;
            break;
        }

        if (ctx->fmt[ctx->cur] != ',')
            return EJSON_BADFORMAT;

        ctx->cur++; // Consume the ","
    }
    
    return EJSON_MATCH;
}

static ejson_matchresult unpack(context_t *ctx, ejson_value *val)
{
    assert(ctx->cur < ctx->len && ctx->fmt[ctx->cur] == '$');

    ctx->cur++; // Consume the "$"
    if (ctx->cur == ctx->len)
        return EJSON_BADFORMAT;
    
    ejson_type expected;

    switch (ctx->fmt[ctx->cur]) {
        case 'a': expected = EJSON_ARRAY;  break;
        case 'o': expected = EJSON_OBJECT; break;
        case 's': expected = EJSON_STRING; break;
        case 'n': expected = EJSON_NUMBER; break;
        case 'b': expected = EJSON_BOOLEAN; break;
        default: return EJSON_BADFORMAT;
    }
    ctx->cur++; // Consume the specifier character

    if (val->type != expected)
        return EJSON_NOMATCH;

    ctx->out[ctx->nout++] = val;
    return EJSON_MATCH;
}

static ejson_matchresult match(context_t *ctx, ejson_value *val)
{
    assert(val->type != EJSON_ARRAY && val->type != EJSON_OBJECT);

    char pool[1024]; // Should suffice for a non-composit type of value

    ejson_value *val2 = parse_next_value_in_fmt(ctx, pool, sizeof(pool));
    if (val == NULL)
        return EJSON_BADFORMAT;

    return ejson_valcmp(val, val2) ? EJSON_MATCH : EJSON_NOMATCH;
}

static ejson_matchresult match_and_unpack(context_t *ctx, ejson_value *val)
{
    consume_spaces(ctx);

    if (ctx->cur == ctx->len)
        return EJSON_BADFORMAT;

    if (ctx->fmt[ctx->cur] == '?') {

        // The "?" matches anything.

        ctx->cur++; // Consume the "?"
        return EJSON_MATCH;
    }

    if (ctx->fmt[ctx->cur] == '$')
        return unpack(ctx, val);

    if (val->type == EJSON_ARRAY)
        return match_and_unpack_arr(ctx, val);

    if (val->type == EJSON_OBJECT)
        return match_and_unpack_obj(ctx, val);

    return match(ctx, val);
}

ejson_matchresult ejson_match_and_unpack(ejson_value *val, const char *fmt, ejson_value **out)
{
    context_t ctx = {
        .fmt=fmt,
        .len=strlen(fmt),
        .cur=0,
        .out=out,
        .nout=0,
    };

    return match_and_unpack(&ctx, val);
}
