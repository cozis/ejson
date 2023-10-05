#include <stdio.h>
#include "ejson.h"

int main(void)
{
    const char src[] = "[97.24, true, {\"name\": false, \"pass\": \"HelloKitty\"}, null]";

    char pool[1 << 16];
    ejson_arena arena = {.base=pool, .size=sizeof(pool), .used=0};
    ejson_error error;
    ejson_value *val = ejson_parse(src, sizeof(src)-1, &error, &arena);
    if (val == NULL) {
        fprintf(stderr, "Error: %s\n", error.msg);
        return -1;
    }
    char output[1 << 10];
    size_t num = ejson_print(val, output, sizeof(output));
    fwrite(output, 1, num, stdout);
    fwrite("\n", 1, 1, stdout);
    return 0;
}
