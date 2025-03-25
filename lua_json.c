#include "hashmap.h"
#include "luaconf.h"
#include <json.h>
#include <lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add(lua_State *L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);
    return 1; // num of results
}

static void push_jval(lua_State *L, struct jvalue *j) {
    struct jvalue **ptr = lua_newuserdata(L, sizeof(void *));
    // get metatable from global and set
    *ptr = j;
    printf("setting %p to %p\n", (void *)ptr, (void *)j);
    luaL_getmetatable(L, "json.val");
    lua_setmetatable(L, -2);
}
static int load(lua_State *L) {
    const char *fn = luaL_checkstring(L, 1);
    char *buf = NULL;
    size_t buf_len = 0;
    struct jerr err = {0};
    struct jvalue *j = load_filename(fn, &buf, &buf_len, &err);
    if (!j) {
        print_jerr_str(&err, buf);
        luaL_error(L, "json erorr\n");
    }

    push_jval(L, j);
    return 1;
}
static int hello(lua_State *L) {
    char *hello = strdup("Heglo!");
    lua_pushstring(L, hello);
    free(hello);
    return 1;
}
static struct jvalue *checkval(lua_State *L) {
    void *ud = luaL_checkudata(L, 1, "json.val");
    luaL_argcheck(L, ud != NULL, 1, "expected json.val");
    struct jvalue **j_ptr = ud;
    struct jvalue *j = *j_ptr;
    return j;
}
static int free_json_obj(lua_State *L) {
    struct jvalue *j = checkval(L);
    // printf("freeing %p. value is: ", (void *)j);
    print_value(j);
    printf("\n");
    free_object(j);
    return 0;
}
static int get_str(lua_State *L) {
    struct jvalue *j = checkval(L);
    if (j->type != JSTR) {
        luaL_error(L, "expected Str but got %s\n", type_to_str(j->type));
    }
    lua_pushstring(L, j->val.str);
    return 1;
}
static int get_num(lua_State *L) {
    struct jvalue *j = checkval(L);
    if (j->type != JNUMBER) {
        luaL_error(L, "expected Number but got %s\n", type_to_str(j->type));
    }
    if (j->val.number.islong) {
        lua_pushinteger(L, (lua_Integer)j->val.number.num.l);
    } else {
        lua_pushinteger(L, (lua_Number)j->val.number.num.d);
    }
    return 1;
}
static int get_bool(lua_State *L) {
    struct jvalue *j = checkval(L);
    if (j->type != JBOOL) {
        luaL_error(L, "expected Bool but got %s\n", type_to_str(j->type));
    }
    lua_pushboolean(L, j->val.boolean);
    return 1;
}
// TODO maybe just do get_* including get_any then convert lua tyeps to json types
static int get(lua_State *L) {
    struct jvalue *j = checkval(L);
    const char *key = luaL_checkstring(L, 2);
    if (j->type != JOBJECT) {
        luaL_error(L, "expected Object but got %s\n", type_to_str(j->type));
    }
    struct jvalue *new = jobj_get(j, key);
    if (!new) {
        lua_pushnil(L);
        return 1;
    }
    // clone so we can free this when it goes out of scope
    // and so the parent can also be freed separately
    struct jvalue *new_copy = jvalue_clone(new);
    push_jval(L, new_copy);
    return 1;
}
static int get_i(lua_State *L) {
    struct jvalue *j = checkval(L);
    size_t i = luaL_checkinteger(L, 2);
    if (j->type != JARRAY) {
        luaL_error(L, "expected Array but got %s\n", type_to_str(j->type));
    }
    struct jvalue *new = jarray_get(j, i);
    if (!new) {
        lua_pushnil(L);
        return 1;
    }
    // clone so we can free this when it goes out of scope
    // and so the parent can also be freed separately
    struct jvalue *new_copy = jvalue_clone(new);
    push_jval(L, new_copy);
    return 1;
}
// TODO
static int get_any_j(lua_State *L, struct jvalue *j);
static int get_obj_j(lua_State *L, struct jvalue *j) {
    lua_newtable(L);
    for (size_t i = 0; i < j->val.obj->len; i++) {
        struct key_pair *pair = j->val.obj->nodes[i].val;
        // k
        lua_pushstring(L, pair->key);
        // v
        get_any_j(L, pair->val);
        // table[k]=v
        lua_settable(L, -3);
    }
    return 1;
}
static int get_arr_j(lua_State *L, struct jvalue *j) {
    lua_newtable(L);
    for (size_t i = 0; i < j->val.array.len; i++) {
        struct jvalue **arr = j->val.array.arr;
        // v
        get_any_j(L, arr[i]);
        // table[i]=v
        lua_seti(L, -3, i + 1);
    }

    return 1;
}
static int get_any_j(lua_State *L, struct jvalue *j) {
    switch (j->type) {
        case UNKNOWN: {
            luaL_error(L, "type is unknown!\n");
            break;
        }
        case JNUMBER: {
            return get_num(L);
        }
        case JBOOL: {
            return get_bool(L);
        }
        case JSTR: {
            return get_str(L);
        }
        case JNULL: {
            lua_pushnil(L);
            return 1;
        }
        case JOBJECT: {
            return get_obj_j(L, j);
        }
        case JARRAY: {
            return get_arr_j(L, j);
        }
    }
    return 1;
}
static int get_any(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_any_j(L, j);
}
static int get_obj(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_obj_j(L, j);
}
static int get_arr(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_arr_j(L, j);
}
static int lua_to_jvalue(lua_State *L) {
    struct jvalue *j = malloc(sizeof(struct jvalue));
    int type = lua_type(L, 1);
    switch (type) {
        case LUA_TNIL: {
            j->type = JNULL;
            break;
        }
        case LUA_TNUMBER: {
            j->type = JNUMBER;
            j->val.number.islong = lua_isinteger(L, 1);
            if (j->val.number.islong) {
                j->val.number.num.l = luaL_checkinteger(L, 1);
            } else {
                j->val.number.num.l = luaL_checknumber(L, 1);
            }
            break;
        }
        case LUA_TTABLE: {
            // check metatable.__isarray
            int isarray_type = luaL_getmetafield(L, 1, "__isarray");
            if (isarray_type == LUA_TNIL) {
                luaL_argerror(L, 1, "expected to have metatable and __isarray");
            }
            bool isarray = lua_toboolean(L, -1);
            // TODO
        }
    }
}

static const struct luaL_Reg json[] = {
    {"add", add},
    {"hello", hello},
    {"load", load},
    {NULL, NULL},
};
static const struct luaL_Reg json_meta[] = {
    {"__gc", free_json_obj},
    // read only ops on jvalues
    // jvalue -> lua
    {"str", get_str},
    {"num", get_num},
    {"bool", get_bool},
    {"arr", get_arr},
    {"obj", get_obj},
    {"any", get_any},
    // index functuons
    {"get", get},
    {"get_i", get_i},
    // lua -> jvalue
    {
        NULL,
        NULL,
    },
};
int luaopen_json(lua_State *L) {
    luaL_newmetatable(L, "json.val");

    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2); // push metatable
    lua_settable(L, -3);  // metatable.__index = metatable

    // lua_pushcfunction(L, free_json_obj);
    // lua_setfield(L, -1, "__gc")
    luaL_setfuncs(L, json_meta, 0);
    luaL_newlib(L, json);
    return 1;
}
