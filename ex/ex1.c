#include <stdio.h>
#include <assert.h>
#include "ejson.h"

int main(void)
{
    const char src[] = "[97.24, true, {\"name\": true, \"pass\": \"HelloKitty\"}, null]";

    char pool[1 << 16];
    ejson_arena arena = {.base=pool, .size=sizeof(pool), .used=0};
    ejson_error error;
    ejson_value *val = ejson_parse(src, sizeof(src)-1, &error, &arena);
    if (val == NULL) {
        fprintf(stderr, "Error: %s\n", error.msg);
        return -1;
    }
    ejson_value *matches[2];
    if (ejson_match_and_unpack(val, "[$n, true, {'name': $b}]", matches)) {
        fprintf(stderr, "Error: Invalid format or no match\n");
        return -1;
    }
    assert(matches[0]->type == EJSON_NUMBER);
    assert(matches[1]->type == EJSON_BOOLEAN);
    fprintf(stderr, "%lld, %s\n", 
        matches[0]->when_number.as_int, 
        matches[1]->when_boolean ? "true" : "false");
    return 0;
}
