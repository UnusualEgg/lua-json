#include "lualib.h"
#include "luaxlib.h"
#include <json.h>
#include <lua.h>
#include <stdio.h>
#include <string.h>

static int add(lua_State *L) {
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    lua_pushnumber(L, a + b);
    return 1; // num of results
}

static int load(lua_State *L) {
    const char *fn = luaL_checkstring(L, 1);
    char *buf = NULL;
    size_t buf_len = 0;
    struct jerr err = {0};
    struct jvalue *j = load_filename(fn, &buf, &buf_len, &err);
    if (!j) {
        print_jerr_str(&err, buf);
        lua_error(L);
    }
    struct jvalue **ptr = lua_newuserdata(L, sizeof(void *));
    *ptr = j;
    return 1;
}
static int hello(lua_State *L) {
    char *hello = strdup("Heglo!");
    lua_pushstring(L, hello);
    free(hello);
    return 1;
}

static const struct luaL_Reg json[] = {
    {"add", add},
    {"hello", hello},
    {NULL, NULL},
};
int luaopen_json(lua_State *L) {
    luaL_newlib(L, json);
    return 1;
}
