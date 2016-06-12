//

#include <ctype.h>

#include "../lua_pd.h"

//

#define JSON_MAX_STACK_SIZE 50

//

static const unsigned char hexdig[] = "0123456789ABCDEF";

static const int dighex[] = {
   [0 ... 255] = -1,
   ['0'] = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
   ['A'] = 10, 11, 12, 13, 14, 15,
   ['a'] = 10, 11, 12, 13, 14, 15
};

//

LUAMOD_API int luaopen_std( lua_State *L );

static int lua_json_encode( lua_State *L );
static int lua_json_decode( lua_State *L );

static int lua_json_decode__scan_value( lua_State *L, const char *str, size_t len, size_t *pos );
static int lua_json_decode__scan_string( lua_State *L, const char *str, size_t len, size_t *pos );
static int lua_json_decode__scan_array( lua_State *L, const char *str, size_t len, size_t *pos );
static int lua_json_decode__scan_object( lua_State *L, const char *str, size_t len, size_t *pos );

static int lua_json_encode__value( lua_State *L, luaL_Buffer *B, int index );
static int lua_json_encode__string( lua_State *L, luaL_Buffer *B, int index );
static int lua_json_encode__array( lua_State *L, luaL_Buffer *B, int index );
static int lua_json_encode__object( lua_State *L, luaL_Buffer *B, int index );
static int lua_json_encode__is_array( lua_State *L, int index );

//

static const luaL_Reg __index[] = {
    {"encode", lua_json_encode},
    {"decode", lua_json_decode},
    {NULL, NULL}
};
