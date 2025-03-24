#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <lua5.3/lauxlib.h>
#include <stdlib.h>
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
    //get metatable from global and set
    *ptr = j;
    printf("setting %p to %p\n",ptr,j);
    luaL_getmetatable(L,"json.val");
    lua_setmetatable(L, -2);

    return 1;
}
static int hello(lua_State *L) {
    char *hello = strdup("Heglo!");
    lua_pushstring(L, hello);
    free(hello);
    return 1;
}
static struct jvalue** checkval(lua_State *L) {
    void* ud = luaL_checkudata(L,1,"json.val");
    luaL_argcheck(L,ud!=NULL,1,"expected json.val");
    return (struct jvalue**)ud;
}
static int free_json_obj(lua_State *L) {
    struct jvalue** ptr = checkval(L);
    struct jvalue* j = *ptr;
    printf("freeing %p. value is:\n");
    print_value(j);
    printf("\n");
    free_object(j);
    return 0;
}

static const struct luaL_Reg json[] = {
    {"add", add},
    {"hello", hello},
    {"load",load},
    {NULL, NULL},
};
static const struct luaL_Reg json_meta[] = {
    {"__gc",free_json_obj},
    {NULL,NULL},
};
int luaopen_json(lua_State *L) {
    luaL_newmetatable(L, "json.val");

    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2); //push metatable
    lua_settable(L, -3); //metatable.__index = metatable

    
    // lua_pushcfunction(L, free_json_obj);
    // lua_setfield(L, -1, "__gc")
    luaL_setfuncs(L, json_meta, 0);
    luaL_newlib(L, json);
    return 1;
}
