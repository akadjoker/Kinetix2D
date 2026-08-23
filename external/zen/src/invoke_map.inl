/*
** invoke_map.inl — Map method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_MAP.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjMap *map = as_map(receiver);

/* Modules are ObjMaps but should NOT expose built-in map methods.
** When is_module is set, jump straight to key lookup. */
if (map->is_module)
    goto map_key_lookup;

#define MAP_METHOD(lit) (method->length == (int)(sizeof(lit) - 1) && memcmp(mname, lit, sizeof(lit) - 1) == 0)

do
{
if (MAP_METHOD("set"))
{
    /* map.set(key, val) → sets key, returns val */
    if (arg_count != 2)
    {
        RT_ERROR("set() expects (key, value)");
    }
    map_set(&gc_, map, args[0], args[1]);
    R[base] = args[1];
    break;
}
if (MAP_METHOD("get"))
{
    /* map.get(key) or map.get(key, default) → value or nil/default */
    if (arg_count < 1)
    {
        RT_ERROR("get() expects a key");
    }
    bool found;
    Value result = map_get(map, args[0], &found);
    if (found)
        R[base] = result;
    else
        R[base] = (arg_count >= 2) ? args[1] : val_nil();
    break;
}
if (MAP_METHOD("has"))
{
    /* map.has(key) → bool */
    if (arg_count != 1)
    {
        RT_ERROR("has() expects a key");
    }
    R[base] = val_bool(map_contains(map, args[0]));
    break;
}
if (MAP_METHOD("delete"))
{
    /* map.delete(key) → removes key, returns true if existed */
    if (arg_count != 1)
    {
        RT_ERROR("delete() expects a key");
    }
    R[base] = val_bool(map_delete(map, args[0]));
    break;
}
if (MAP_METHOD("keys"))
{
    /* map.keys() → array of keys */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    map_keys(&gc_, map, as_array(R[base]));
    break;
}
if (MAP_METHOD("values"))
{
    /* map.values() → array of values */
    ObjArray *result = new_array(&gc_);
    R[base] = val_obj((Obj *)result);
    map_values(&gc_, map, as_array(R[base]));
    break;
}
if (MAP_METHOD("items"))
{
    /* map.items() → array of [key, value] pairs */
    gc_pause(&gc_);
    ObjArray *result = new_array(&gc_);
    for (int32_t mi = 0; mi < map->capacity; mi++)
    {
        if (map->nodes[mi].hash != 0xFFFFFFFFu)
        {
            ObjArray *pair = new_array(&gc_);
            array_push(&gc_, pair, map->nodes[mi].key);
            array_push(&gc_, pair, map->nodes[mi].value);
            array_push(&gc_, result, val_obj((Obj *)pair));
        }
    }
    R[base] = val_obj((Obj *)result);
    gc_resume(&gc_);
    break;
}
if (MAP_METHOD("size"))
{
    /* map.size() → number of entries */
    R[base] = val_int(map->count);
    break;
}
if (MAP_METHOD("clear"))
{
    /* map.clear() → remove all entries */
    map_clear(&gc_, map);
    R[base] = val_nil();
    break;
}
if (MAP_METHOD("dump"))
{
    /* map.dump() → pretty-print contents recursively */
    dump_value_rec(receiver, 0);
    putchar('\n');
    R[base] = val_nil();
    break;
}
{
map_key_lookup:
    /* Not a built-in map method — check if the map contains a callable
    ** with this name (module function dispatch: math.sin(x)) */
    ObjString *key = intern_string(&gc_, mname, (int)strlen(mname),
                                   hash_string(mname, (int)strlen(mname)));
    bool found;
    Value callable = map_get(map, val_obj((Obj *)key), &found);
    if (found && is_native(callable))
    {
        ObjNative *nat = as_native(callable);
        /* Module functions don't take receiver (no self) */
        int nret = call_native(this, nat->fn, args, arg_count);
        if (nret >= 0)
            copy_native_results(&R[base], args, nret, nresults);
        else
            RT_ERROR("native function '%s' returned error", mname);
        break;
    }
    if (found && is_closure(callable))
    {
        /* Module-level closure — call without self */
        ObjClosure *cl = as_closure(callable);
        ObjFunc *fn = cl->func;
        if (fn->arity >= 0 && arg_count != fn->arity)
            RT_ERROR("%s() expects %d args but got %d", mname, fn->arity, arg_count);
        if (fiber->frame_count >= kMaxFrames)
            RT_ERROR("stack overflow");
        /* Shift args down: args[0..nargs-1] become base[0..nargs-1] */
        for (int ai = 0; ai < arg_count; ai++)
            R[base + ai] = args[ai];
        ++ip;
        SAVE_IP();
        CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
        new_frame->closure = cl;
        new_frame->func = fn;
        new_frame->ip = fn->code;
        new_frame->base = &R[base];
        new_frame->ret_reg = base;
        new_frame->ret_count = 1;
        fiber->stack_top = new_frame->base + fn->num_regs;
        LOAD_STATE();
        DISPATCH();
    }
    RT_ERROR("map has no method or key '%s'", mname);
}
} while (0);

#undef MAP_METHOD
