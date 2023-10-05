#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ejson.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    char *dst;
    size_t num, max;
} print_context_t;

static void append(print_context_t *ctx, const char *str, size_t len)
{
    if (ctx->num < ctx->max) {
        size_t cpy = MIN(len, ctx->max - ctx->num);
        memcpy(ctx->dst + ctx->num, str, cpy);
    }
    ctx->num += len;
}

static void print_str(print_context_t *ctx, ejson_string str)
{
    append(ctx, "\"", 1);
    append(ctx, str.base, str.size);
    append(ctx, "\"", 1);
}

static void print_any(print_context_t *ctx, ejson_value *val)
{
    switch (val->type) {
        
        case EJSON_NULL: 
        append(ctx, "null", 4); 
        break;
        
        case EJSON_ARRAY:
        append(ctx, "[", 1);
        for (ejson_iter iter = ejson_iterover(val); ejson_next(&iter); ) {
            print_any(ctx, iter.val);
            if (ejson_hasnext(iter.val))
                append(ctx, ", ", 2);
        }
        append(ctx, "]", 1);
        break;

        case EJSON_OBJECT:
        append(ctx, "{", 1);
        for (ejson_iter iter = ejson_iterover(val); ejson_next(&iter); ) {
            print_str(ctx, iter.key);
            append(ctx, ": ", 2);
            print_any(ctx, iter.val);
            if (ejson_hasnext(iter.val))
                append(ctx, ", ", 2);
        }
        append(ctx, "}", 1);
        break;
        
        case EJSON_NUMBER:
        {
            char buff[128];
            double  flt = val->when_number.as_flt;
            int64_t itg = val->when_number.as_int;
            int n;
            if (flt == itg)
                n = snprintf(buff, sizeof(buff), "%lld", val->when_number.as_int);
            else
                n = snprintf(buff, sizeof(buff), "%lf", val->when_number.as_flt);
            assert(n > 0);
            append(ctx, buff, n);
        }
        break;
        
        case EJSON_STRING:
        append(ctx, "\"", 1);
        append(ctx, val->when_string.base, val->when_string.size); 
        append(ctx, "\"", 1);
        break;
        
        case EJSON_BOOLEAN:
        if (val->when_boolean)
            append(ctx, "true", 4); 
        else
            append(ctx, "false", 5);
        break;
    }
}

size_t ejson_print(ejson_value *val, char *dst, size_t max)
{
    print_context_t ctx = {
        .dst=dst,
        .max=max,
        .num=0,
    };
    print_any(&ctx, val);
    size_t num = ctx.num;

    if (num < max)
        dst[num] = '\0';
    else
        dst[max-1] = '\0';
    
    return num;
}