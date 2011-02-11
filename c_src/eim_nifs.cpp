#include "eim_nifs.h"
#include "erl_nif_compat.h"
#include <string.h>

ERL_NIF_TERM load_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ErlNifBinary data;
    if(enif_inspect_binary(env, argv[0], &data))
    {
        try
        {
            ihandle* handle = (ihandle*)enif_alloc_resource_compat(env, EIM_IMAGE_RESOURCE, sizeof(ihandle));
            handle->image = new eim_image(data.data, data.size);
            ERL_NIF_TERM result = enif_make_resource(env, handle);
            enif_release_resource_compat(env, handle);
            return enif_make_tuple2(env, enif_make_atom(env, "ok"), result);
        }
        catch(const char* msg)
        {
            return enif_make_atom(env, "error");
        }
    }
    else
    {
        return enif_make_badarg(env);
    }
}
ERL_NIF_TERM derive_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ihandle* handle;
    ERL_NIF_TERM head;
    ERL_NIF_TERM tail;
    if(enif_get_resource(env, argv[0], EIM_IMAGE_RESOURCE, (void**)&handle)
    && enif_get_list_cell(env, argv[1], &head, &tail))
    {
        ErlNifBinary new_binary;
        size_t new_length;
        unsigned char *new_blob;
        
        do {
            //todo: error handle
            int tuplec;
            const ERL_NIF_TERM* tuple;
            char type[9];
            if(!enif_get_tuple(env, head, &tuplec, &tuple)
            || !enif_get_atom_compat(env, tuple[0], type, 9, ERL_NIF_LATIN1))
            {
                return enif_make_badarg(env);
            }
            long x, y;
            long width, height;
            long value;
            char dimension[7];
            switch(type[0])
            {
                case 's'://scale
                    if(!enif_get_atom_compat(env, tuple[1], dimension, 7, ERL_NIF_LATIN1)
                    || !enif_get_long(env, tuple[2], &value))
                    {
                        return enif_make_badarg(env);
                    }
                    if(dimension[0]=='w')
                    {
                        handle->image->scale_width(value);
                    }
                    else if(dimension[0]=='h')
                    {
                        handle->image->scale_height(value);
                    }
                    else
                    {
                        return enif_make_badarg(env);
                    }
                    break;
                case 'c'://crop
                    if(!enif_get_long(env, tuple[1], &width) || !enif_get_long(env, tuple[2], &height)
                    || !enif_get_long(env, tuple[3], &x) || !enif_get_long(env, tuple[4], &y))
                    {
                        return enif_make_badarg(env);
                    }
                    handle->image->crop(width, height, x, y);
                    break;
                case 'm'://max
                    if(!enif_get_atom_compat(env, tuple[1], dimension, 7, ERL_NIF_LATIN1)
                    || !enif_get_long(env, tuple[2], &value))
                    {
                        return enif_make_badarg(env);
                    }
                    if(dimension[0]=='w')
                    {
                        handle->image->max_width(value);
                    }
                    else if(dimension[0]=='h')
                    {
                        handle->image->max_height(value);
                    }
                    else
                    {
                        return enif_make_badarg(env);
                    }
                    break;
                case 'f'://fit
                    if(!enif_get_long(env, tuple[1], &width) || !enif_get_long(env, tuple[2], &height))
                    {
                        return enif_make_badarg(env);
                    }
                    handle->image->fit(width, height);
                    break;
                case 'b'://box
                    if(!enif_get_long(env, tuple[1], &width) || !enif_get_long(env, tuple[2], &height))
                    {
                        return enif_make_badarg(env);
                    }
                    char float_x[7], float_y[7];
                    char floated;
                    if(enif_get_atom_compat(env, tuple[3], float_x, 7, ERL_NIF_LATIN1))
                    {
                        if(!enif_get_atom_compat(env, tuple[4], float_y, 7, ERL_NIF_LATIN1))
                        {
                            return enif_make_badarg(env);
                        }
                        // can actually write top,left instead of left,top if you want
                        // just helps with typos like that. Just works.
                        switch(float_x[0])
                        {
                            case 'l': floated = EIM_FLOAT_LEFT; break;
                            case 'c': floated = EIM_FLOAT_CENTER; break;
                            case 'r': floated = EIM_FLOAT_RIGHT; break;
                            case 't': floated = EIM_FLOAT_TOP; break;
                            case 'b': floated = EIM_FLOAT_BOTTOM; break;
                            default:
                                return enif_make_badarg(env);
                        }
                        switch(float_y[0])
                        {
                            case 'l': floated ^= EIM_FLOAT_LEFT; break;
                            case 'c': floated ^= EIM_FLOAT_CENTER; break;
                            case 'r': floated ^= EIM_FLOAT_RIGHT; break;
                            case 't': floated ^= EIM_FLOAT_TOP; break;
                            case 'b': floated ^= EIM_FLOAT_BOTTOM; break;
                            default:
                                return enif_make_badarg(env);
                        }
                    }
                    else
                    {
                        floated = EIM_FLOAT_CENTER | EIM_FLOAT_CENTER;
                    }
                    handle->image->box(width, height, floated);
                    break;
                default:
                    return enif_make_badarg(env);
            }
            
        } while(enif_get_list_cell(env, tail, &head, &tail));
        
        try
        {
            new_blob = handle->image->process(&new_length);
            enif_alloc_binary_compat(env, new_length, &new_binary);
            memcpy(new_binary.data, new_blob, new_length);
            return enif_make_binary(env, &new_binary);
        }
        catch(const char* msg)
        {
            return enif_make_atom(env, "error");
        }
    }
    else
    {
        return enif_make_badarg(env);
    }
}

int on_load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
{
    ErlNifResourceFlags flags = (ErlNifResourceFlags)(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER);
    EIM_IMAGE_RESOURCE = enif_open_resource_type_compat(env, "eim_image_resource", NULL, flags, 0);
    return 0;
}


