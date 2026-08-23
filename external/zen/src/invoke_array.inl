/*
** invoke_array.inl — Array method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_ARRAY.
**
** Available variables:
**   base      — register index of receiver (R[base] = array)
**   arg_count — number of arguments
**   receiver  — Value of the array
**   mname     — method name (const char*)
**   mlen      — method name length
**   args      — pointer to first argument (R[base+1])
**   R         — register file
**   K         — constant pool
*/

ObjArray *arr = as_array(receiver);

#define ARRAY_METHOD(lit) (method->length == (int)(sizeof(lit) - 1) && memcmp(mname, lit, sizeof(lit) - 1) == 0)

do
{
if (ARRAY_METHOD("push") || ARRAY_METHOD("append"))
{
    /* arr.push(val) → append, returns new length */
    if (arg_count < 1)
    {
        RT_ERROR("push() expects at least 1 argument");
    }
    for (int ai = 0; ai < arg_count; ai++)
        array_push(&gc_, arr, args[ai]);
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("pop"))
{
    /* arr.pop() → remove+return last element */
    if (arr_count(arr) == 0)
    {
        RT_ERROR("pop() on empty array");
    }
    R[base] = *--arr->end;
    break;
}
if (ARRAY_METHOD("len"))
{
    /* arr.len() → length */
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("remove"))
{
    /* arr.remove(idx) → remove at index, return removed value */
    if (arg_count != 1 || !is_int(args[0]))
    {
        RT_ERROR("remove() expects 1 integer argument");
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx >= count)
    {
        RT_ERROR("remove() index out of bounds");
    }
    Value removed = arr->data[idx];
    memmove(&arr->data[idx], &arr->data[idx + 1], (size_t)(count - idx - 1) * sizeof(Value));
    arr->end--;
    R[base] = removed;
    break;
}
if (ARRAY_METHOD("insert"))
{
    /* arr.insert(idx, val) → insert at position */
    if (arg_count != 2 || !is_int(args[0]))
    {
        RT_ERROR("insert() expects (int, value)");
    }
    int32_t idx = args[0].as.integer;
    int32_t count = arr_count(arr);
    if (idx < 0 || idx > count)
    {
        RT_ERROR("insert() index out of bounds");
    }
    array_push(&gc_, arr, val_nil()); /* ensure capacity, end++ */
    /* shift right */
    memmove(&arr->data[idx + 1], &arr->data[idx], (size_t)(count - idx) * sizeof(Value));
    arr->data[idx] = args[1];
    R[base] = val_int(arr_count(arr));
    break;
}
if (ARRAY_METHOD("slice"))
{
    /* arr.slice(start, end?) → new array [start, end) */
    int32_t count = arr_count(arr);
    int32_t start = 0, end_idx = count;
    if (arg_count >= 1 && is_int(args[0]))
        start = args[0].as.integer;
    if (arg_count >= 2 && is_int(args[1]))
        end_idx = args[1].as.integer;
    if (start < 0)
        start += count;
    if (end_idx < 0)
        end_idx += count;
    if (start < 0)
        start = 0;
    if (end_idx > count)
        end_idx = count;
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result); /* root before reserve triggers GC */
    if (start < end_idx)
    {
        int32_t new_count = end_idx - start;
        array_reserve(&gc_, as_array(R[base]), new_count);
        result = as_array(R[base]);
        memcpy(result->data, &arr->data[start], (size_t)new_count * sizeof(Value));
        result->end = result->data + new_count;
    }
    break;
}
if (ARRAY_METHOD("reverse"))
{
    /* arr.reverse() → in-place reverse, returns arr */
    array_reverse(arr);
    R[base] = receiver;
    break;
}
if (ARRAY_METHOD("clear"))
{
    /* arr.clear() → empty the array */
    array_clear(arr);
    R[base] = val_nil();
    break;
}
if (ARRAY_METHOD("contains"))
{
    /* arr.contains(val) → bool */
    if (arg_count != 1)
    {
        RT_ERROR("contains() expects 1 argument");
    }
    R[base] = val_bool(array_contains(arr, args[0]));
    break;
}
if (ARRAY_METHOD("join"))
{
    /* arr.join(sep?) → string */
    const char *sep = "";
    int sep_len = 0;
    if (arg_count >= 1 && is_string(args[0]))
    {
        sep = as_cstring(args[0]);
        sep_len = as_string(args[0])->length;
    }
    int32_t count = arr_count(arr);
    char num_buf[64];
    /* First pass: compute length */
    int total_len = 0;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
            total_len += sep_len;
        Value v = arr->data[ji];
        if (is_string(v))
            total_len += as_string(v)->length;
        else if (is_int(v))
            total_len += int_to_cstr(v.as.integer, num_buf);
        else if (is_float(v))
            total_len += snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
        else if (is_nil(v))
            total_len += 3;
        else if (is_bool(v))
            total_len += v.as.boolean ? 4 : 5;
        else
            total_len += 3;
    }
    /* Second pass: build string */
    char *buf = (char *)malloc(total_len + 1);
    char *p = buf;
    for (int32_t ji = 0; ji < count; ji++)
    {
        if (ji > 0)
        {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        Value v = arr->data[ji];
        if (is_string(v))
        {
            memcpy(p, as_cstring(v), as_string(v)->length);
            p += as_string(v)->length;
        }
        else if (is_int(v))
        {
            int n = int_to_cstr(v.as.integer, num_buf);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_float(v))
        {
            int n = snprintf(num_buf, sizeof(num_buf), "%g", v.as.number);
            memcpy(p, num_buf, n);
            p += n;
        }
        else if (is_nil(v))
        {
            memcpy(p, "nil", 3);
            p += 3;
        }
        else if (is_bool(v))
        {
            const char *s = v.as.boolean ? "true" : "false";
            int n = v.as.boolean ? 4 : 5;
            memcpy(p, s, n);
            p += n;
        }
        else
        {
            memcpy(p, "???", 3);
            p += 3;
        }
    }
    R[base] = val_obj((Obj *)create_string(&gc_, buf, total_len));
    free(buf);
    break;
}
if (ARRAY_METHOD("sort"))
{
    /* arr.sort() or arr.sort("desc") → in-place sort using qsort */
    if (arg_count > 1)
    {
        RT_ERROR("sort() expects 0 or 1 argument");
    }
    bool descending = false;
    if (arg_count == 1)
    {
        if (!is_string(args[0]))
        {
            RT_ERROR("sort() argument must be a string (\"asc\" or \"desc\")");
        }
        ObjString *order = as_string(args[0]);
        if (order->length == 4 && memcmp(order->chars, "desc", 4) == 0)
            descending = true;
    }
    int32_t count = arr_count(arr);
    if (count > 1)
    {
        /* qsort with static comparator — store direction in a thread-local (ok for single-threaded VM) */
        static bool s_desc;
        s_desc = descending;
        qsort(arr->data, (size_t)count, sizeof(Value), [](const void *pa, const void *pb) -> int
              {
            Value a = *(const Value *)pa, b = *(const Value *)pb;
            int cmp = 0;
            double da = 0, db = 0;
            bool a_num = is_int(a) || is_float(a);
            bool b_num = is_int(b) || is_float(b);
            if (a_num && b_num) {
                da = is_int(a) ? (double)a.as.integer : a.as.number;
                db = is_int(b) ? (double)b.as.integer : b.as.number;
                cmp = (da > db) - (da < db);
            } else if (is_string(a) && is_string(b)) {
                int minlen = as_string(a)->length < as_string(b)->length ? as_string(a)->length : as_string(b)->length;
                cmp = memcmp(as_cstring(a), as_cstring(b), minlen);
                if (cmp == 0) cmp = as_string(a)->length - as_string(b)->length;
            } else {
                /* numbers before strings before others */
                cmp = (int)a.type - (int)b.type;
            }
            return s_desc ? -cmp : cmp; });
    }
    R[base] = receiver;
    break;
}
if (ARRAY_METHOD("index_of") || ARRAY_METHOD("index"))
{
    /* arr.index_of(val) → index or -1 */
    if (arg_count != 1)
    {
        RT_ERROR("index_of() expects 1 argument");
    }
    R[base] = val_int(array_find(arr, args[0]));
    break;
}
if (ARRAY_METHOD("dump"))
{
    /* arr.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
{
    RT_ERROR("array has no method '%s'", mname);
}
} while (0);

#undef ARRAY_METHOD
