#include "hashmap.h"
#include <json.h>
#include <lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <stdbool.h>
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
    // printf("setting %p to %p\n", (void *)ptr, (void *)j);
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
        jerr_print_str(&err, buf);
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
    // print_value(j);
    // printf("\n");
    free_object(j);
    return 0;
}
static int get_str_j(lua_State *L, struct jvalue *j) {
    enum jtype expected = JSTR;
    const enum jtype got = j->type;

    if (got != expected) {
        luaL_error(L, "expected %s but got %s\n", jtype_to_str(expected), jtype_to_str(got));
    }
    lua_pushstring(L, j->val.str);
    return 1;
}
static int get_num_j(lua_State *L, struct jvalue *j) {
    enum jtype expected = JNUMBER;
    const enum jtype got = j->type;

    if (got != expected) {
        luaL_error(L, "expected %s but got %s\n", jtype_to_str(expected), jtype_to_str(got));
    }
    if (j->val.number.islong) {
        lua_pushinteger(L, (lua_Integer)j->val.number.num.l);
    } else {
        lua_pushinteger(L, (lua_Number)j->val.number.num.d);
    }
    return 1;
}
static int get_bool_j(lua_State *L, struct jvalue *j) {
    enum jtype expected = JBOOL;
    const enum jtype got = j->type;

    if (got != expected) {
        luaL_error(L, "expected %s but got %s\n", jtype_to_str(expected), jtype_to_str(got));
    }
    lua_pushboolean(L, j->val.boolean);
    return 1;
}
static int get(lua_State *L) {
    struct jvalue *j = checkval(L);
    const char *key = luaL_checkstring(L, 2);
    enum jtype expected = JOBJECT;
    const enum jtype got = j->type;

    if (got != expected) {
        luaL_error(L, "expected %s but got %s\n", jtype_to_str(expected), jtype_to_str(got));
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
    enum jtype expected = JARRAY;
    const enum jtype got = j->type;

    if (got != expected) {
        luaL_error(L, "expected %s but got %s\n", jtype_to_str(expected), jtype_to_str(got));
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
static int get_any_j(lua_State *L, struct jvalue *j);
static int get_obj_j(lua_State *L, struct jvalue *j) {
    lua_newtable(L);
    struct hashmap_node *n = NULL;
    for (size_t i = 0; i < j->val.obj->len; i++) {
        if (!n) {
            n = j->val.obj->nodes;
        }
        struct key_pair *pair = n->val;
        // v
        get_any_j(L, pair->val);
        // table[k]=v
        lua_setfield(L, -2, pair->key);
        n = n->next;
    }
    lua_newtable(L);
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "__isarray");
    lua_setmetatable(L, -2);
    return 1;
}
static int get_arr_j(lua_State *L, struct jvalue *j) {
    lua_newtable(L);
    for (size_t i = 0; i < j->val.array.len; i++) {
        struct jvalue **arr = j->val.array.arr;
        // v
        get_any_j(L, arr[i]);
        // table[i]=v
        lua_seti(L, -2, i + 1);
    }

    lua_newtable(L);
    lua_pushboolean(L, true);
    lua_setfield(L, -2, "__isarray");
    lua_setmetatable(L, -2);
    return 1;
}
static int get_any_j(lua_State *L, struct jvalue *j) {
    switch (j->type) {
        case UNKNOWN: {
            luaL_error(L, "type is unknown!\n");
            break;
        }
        case JNUMBER: {
            return get_num_j(L, j);
        }
        case JBOOL: {
            return get_bool_j(L, j);
        }
        case JSTR: {
            // printf("is string + %d %s\n", j->type, jtype_to_str(j->type));
            return get_str_j(L, j);
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
static int get_str(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_str_j(L, j);
}
static int get_num(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_num_j(L, j);
}
static int get_bool(lua_State *L) {
    struct jvalue *j = checkval(L);
    return get_bool_j(L, j);
}
static void lua_to_jvalue_j(lua_State *L, int idx, struct jvalue *j) {
    int type = lua_type(L, idx);
    // printf("[lua_to_jvalue_j]type:%s at %d\n", lua_typename(L, type), idx);
    switch (type) {
        case LUA_TNIL: {
            j->type = JNULL;
            break;
        }
        case LUA_TNUMBER: {
            j->type = JNUMBER;
            bool islong = lua_isinteger(L, idx);
            j->val.number.islong = islong;
            if (islong) {
                j->val.number.num.l = luaL_checkinteger(L, idx);
            } else {
                j->val.number.num.l = luaL_checknumber(L, idx);
            }
            break;
        }
        case LUA_TBOOLEAN: {
            j->type = JBOOL;
            j->val.boolean = lua_toboolean(L, idx);
            break;
        }
        case LUA_TSTRING: {
            j->type = JSTR;
            j->val.str = strdup(luaL_checkstring(L, idx));
            // printf("made string\n");
            break;
        }
        case LUA_TTABLE: {
            // check metatable.__isarray
            int isarray_type = luaL_getmetafield(L, idx, "__isarray");
            if (isarray_type == LUA_TNIL) {
                luaL_argerror(L, 1, "expected to have metatable and __isarray");
            }
            bool isarray = lua_toboolean(L, -1);
            // printf("isarray: %s\n", isarray ? "true" : "false");
            lua_pop(L, 1);
            if (isarray) {
                j->type = JARRAY;

                lua_len(L, idx);
                size_t len = lua_tonumber(L, -1);

                for (size_t i = 0; i < len; i++) {
                    int elm_type = lua_geti(L, idx, i);
                    if (elm_type == LUA_TNIL)
                        break;
                    struct jvalue *new_element = malloc(sizeof(struct jvalue));
                    lua_to_jvalue_j(L, lua_gettop(L), new_element);
                    j->val.array.arr[i] = new_element;
                }
                j->val.array.len = len;
            } else {
                j->type = JOBJECT;
                j->val.obj = hm_create();

                lua_pushnil(L);
                while (lua_next(L, 1) != 0) {
                    // stack: -2:key -1:value
                    size_t len = 0;
                    const char *key = luaL_checklstring(L, -2, &len);
                    struct jvalue *new_element = malloc(sizeof(struct jvalue));
                    lua_to_jvalue_j(L, lua_gettop(L), new_element);
                    jobj_set(j, key, new_element);
                    // pop value
                    lua_pop(L, 1);
                }
            }
            break;
        }
        case LUA_TFUNCTION:
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
        case LUA_TTHREAD: {
            luaL_error(L, "Expected compatible jvalue lua type but got %s\n",
                       lua_typename(L, type));
        }
    }
    printf("[lua_to_jvalue_j] done: ");
    print_value(j);
    printf("\n");
}
static int lua_to_jvalue(lua_State *L) {
    // FIXME
    struct jvalue *j = malloc(sizeof(struct jvalue));
    if (!j)
        luaL_error(L, "failed to malloc jvalue");
    lua_to_jvalue_j(L, 1, j);
    push_jval(L, j);
    return 1;
}
static int jvalue_to_str(lua_State *L) {
    struct jvalue *j = checkval(L);
    char *s = sprint_value_normal(j);
    // printf("[jvalue_to_str]: %s\n", s);
    lua_pushstring(L, s);
    free(s);
    return 1;
}
void set_isarray(struct lua_State *L, int idx, bool isarray) {
    bool has_metatable = lua_getmetatable(L, idx);
    if (!has_metatable) {
        lua_newtable(L);
    }
    lua_pushboolean(L, isarray);
    lua_setfield(L, idx, "__isarray");
    lua_setmetatable(L, -2);
}
static int lua_mark_as_jarray(struct lua_State *L) {
    int type = lua_type(L, 1);
    luaL_argcheck(L, type == LUA_TTABLE, 1, "expected table");
    set_isarray(L, 1, true);
    return 0;
}
static int lua_mark_as_jobject(struct lua_State *L) {
    int type = lua_type(L, 1);
    luaL_argcheck(L, type == LUA_TTABLE, 1, "expected table");
    set_isarray(L, 1, false);
    return 0;
}

static const struct luaL_Reg json[] = {
    {"add", add},
    {"hello", hello},
    {"load", load},
    // lua -> jvalue
    {"lua_to_jvalue", lua_to_jvalue},
    // required because objects and arrays are tables in lua
    {"lua_mark_jarray", lua_mark_as_jarray},
    {"lua_mark_jobject", lua_mark_as_jobject},
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
    {"__tostring", jvalue_to_str},
    {NULL, NULL},
};
int luaopen_json(lua_State *L) {
    luaL_newmetatable(L, "json.val");

    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2); // push metatable
    lua_settable(L, -3);  // metatable.__index = metatable
    luaL_setfuncs(L, json_meta, 0);

    luaL_newmetatable(L, "json.table");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "__isarray");

    luaL_newlib(L, json);
    return 1;
}
