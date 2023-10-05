#include <assert.h>
#include <string.h>
#include "ejson.h"

bool ejson_valcmp(ejson_value *v1, ejson_value *v2)
{
    if (v1->type != v2->type)
        return false;

    switch (v1->type) {
        
        case EJSON_NULL:
        return true;

        case EJSON_ARRAY:
        case EJSON_OBJECT:
        {
            if (v1->when_array.size != v2->when_array.size)
                return false;
            ejson_value *cur1 = v1->when_array.head;
            ejson_value *cur2 = v2->when_array.head;
            while (cur1) {
                assert(cur2);
                if (!ejson_valcmp(cur1, cur2))
                    return false;
                cur1 = cur1->next;
                cur2 = cur2->next;
            }
            return true;
        }
        
        case EJSON_NUMBER:
        return v1->when_number.as_int == v2->when_number.as_int
            && v1->when_number.as_flt == v2->when_number.as_flt;
        
        case EJSON_STRING:
        return v1->when_string.size == v2->when_string.size
            && !strncmp(v1->when_string.base, v2->when_string.base, v1->when_string.size);

        case EJSON_BOOLEAN:
        return v1->when_boolean == v2->when_boolean;
    }
    return false;
}