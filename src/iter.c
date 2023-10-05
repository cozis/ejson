#include <assert.h>
#include "ejson.h"

ejson_iter ejson_iterover(ejson_value *set)
{
    ejson_iter iter;
    iter.set = set;
    iter.val = NULL;
    iter.idx = 0;
    iter.key.base = NULL;
    iter.key.size = 0;
    return iter;
}

bool ejson_next(ejson_iter *iter)
{
    ejson_value *set = iter->set;
    assert(set->type == EJSON_ARRAY || set->type == EJSON_OBJECT);

    if (iter->val == NULL) {
        iter->val = set->when_array.head;
        iter->idx = 0;
    } else {
        iter->val = iter->val->next;
        iter->idx++;
    }
    if (iter->val)
        iter->key = iter->val->key;
    return iter->val != NULL;
}

bool ejson_hasnext(ejson_value *val)
{
    return val->next != NULL;
}