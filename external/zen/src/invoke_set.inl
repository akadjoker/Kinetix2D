/*
** invoke_set.inl — Set method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_SET.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjSet *set = as_set(receiver);

#define SET_METHOD(lit) (method->length == (int)(sizeof(lit) - 1) && memcmp(mname, lit, sizeof(lit) - 1) == 0)

do
{
if (SET_METHOD("add"))
{
    /* set.add(val) → returns true if newly added */
    if (arg_count != 1) { RT_ERROR("add() expects 1 argument"); }
    R[base] = val_bool(set_add(&gc_, set, args[0]));
    break;
}
if (SET_METHOD("has"))
{
    /* set.has(val) → bool */
    if (arg_count != 1) { RT_ERROR("has() expects 1 argument"); }
    R[base] = val_bool(set_contains(set, args[0]));
    break;
}
if (SET_METHOD("delete"))
{
    /* set.delete(val) → returns true if was present */
    if (arg_count != 1) { RT_ERROR("delete() expects 1 argument"); }
    R[base] = val_bool(set_remove(set, args[0]));
    break;
}
if (SET_METHOD("size"))
{
    /* set.size() → number of elements */
    R[base] = val_int(set->count);
    break;
}
if (SET_METHOD("clear"))
{
    /* set.clear() → remove all elements */
    set_clear(&gc_, set);
    R[base] = val_nil();
    break;
}
if (SET_METHOD("values"))
{
    /* set.values() → array of all values */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    for (int32_t si = 0; si < set->capacity; si++) {
        if (set->nodes[si].hash != 0xFFFFFFFFu) {
            array_push(&gc_, as_array(R[base]), set->nodes[si].key);
        }
    }
    break;
}
if (SET_METHOD("dump"))
{
    /* set.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
{
    RT_ERROR("set has no method '%s'", mname);
}
} while (0);

#undef SET_METHOD
