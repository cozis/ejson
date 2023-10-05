#ifndef EJSON_H
#define EJSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ejson_value ejson_value;

typedef struct {
    void  *base;
    size_t size;
    size_t used;
} ejson_arena;

typedef struct {
    char msg[512];
} ejson_error;

typedef enum {
    EJSON_NULL,
    EJSON_ARRAY,
    EJSON_OBJECT,
    EJSON_NUMBER,
    EJSON_STRING,
    EJSON_BOOLEAN,
} ejson_type;

typedef struct {
    const char  *base;
    size_t       size;
} ejson_string;

typedef struct {
    int64_t as_int;
    double  as_flt;
} ejson_number;

typedef struct {
    ejson_value *head;
    size_t       size;
} ejson_array;

struct ejson_value {
    ejson_value **prev;
    ejson_value  *next;
    ejson_string  key;
    ejson_type    type;
    union {
        ejson_array  when_array;
        ejson_number when_number;
        ejson_string when_string;
        bool         when_boolean;
    };
};

typedef struct {
    ejson_value *set;
    ejson_value *val;
    size_t       idx;
    ejson_string key;
} ejson_iter;

typedef struct {
    bool allow_single_quoted_strings;
} ejson_config;

typedef enum {
    EJSON_MATCH     =  0,
    EJSON_NOMATCH   =  1,
    EJSON_BADFORMAT = -1,
} ejson_matchresult;

#define EJSON_DEFAULT_CONFIGS ((ejson_config) { \
        .allow_single_quoted_strings=false,     \
    })

ejson_value *ejson_seekbykey (ejson_value *value, const char *key);
ejson_value *ejson_seekbykey2(ejson_value *value, const char *key, size_t size);

ejson_value *ejson_parse2(const char *src, size_t len, size_t *end,
                          ejson_error *error, ejson_arena *arena,
                          ejson_config config);

ejson_value *ejson_parse(const char *src, size_t len,
                         ejson_error *error, ejson_arena *arena);

bool   ejson_valcmp(ejson_value *v1, ejson_value *v2);
size_t ejson_print(ejson_value *val, char *dst, size_t max);

bool       ejson_next(ejson_iter *iter);
bool       ejson_hasnext(ejson_value *val);
ejson_iter ejson_iterover(ejson_value *set);

ejson_matchresult ejson_match_and_unpack(ejson_value *val, const char *fmt, ejson_value **out);

#endif