//

#include "lua_json.h"

//

LUAMOD_API int luaopen_json( lua_State *L ) {
    luaL_newlib(L, __index);
    return 1;
}

static int lua_json_encode( lua_State *L ) {
    luaL_Buffer B;
    luaL_buffinit(L, &B);

    int r = lua_json_encode__value(L, &B, 1);

    if ( r == 3 ) { // error
        return 3;
    } else {
        luaL_pushresult(&B);
        return 1;
    }
}

static int lua_json_encode__value( lua_State *L, luaL_Buffer *B, int index ) {
    int type = lua_type(L, index);

    switch ( type ) {
        case LUA_TNONE:
        case LUA_TNIL:
            luaL_addstring(B, "null");
            return 1;
        case LUA_TNUMBER:
            lua_pushvalue(L, index);
            luaL_addvalue(B);
            return 1;
        case LUA_TBOOLEAN:
            if ( lua_toboolean(L, index) ) {
                luaL_addstring(B, "true");
            } else {
                luaL_addstring(B, "false");
            }
            return 1;
        case LUA_TTHREAD:
        case LUA_TUSERDATA:
        case LUA_TFUNCTION:
            lua_pushfstring(L, "%s: %p", lua_typename(L, type), lua_topointer(L, index));
            luaL_addvalue(B);
            return 1;
        case LUA_TLIGHTUSERDATA:
            lua_pushfstring(L, "lightuserdata: %p", lua_topointer(L, index));
            luaL_addvalue(B);
            return 1;
        case LUA_TSTRING:
            return lua_json_encode__string(L, B, index);
        case LUA_TTABLE:
            if ( lua_json_encode__is_array(L, index) ) {
                return lua_json_encode__array(L, B, index);
            } else {
                return lua_json_encode__object(L, B, index);
            }
    }

    lua_fail_f(L, "unknown type: %d", 0, type);
}

static int lua_json_encode__is_array( lua_State *L, int index ) {
    int expect_next_idx = 1;

    lua_pushnil(L);
    while ( lua_next(L, index) ) {
        if ( lua_type(L, -2) != LUA_TNUMBER || expect_next_idx++ != lua_tonumber(L, -2) ) {
            lua_pop(L, 2);
            return 0;
        }

        lua_pop(L, 1);
    }

    return expect_next_idx > 1;
}

static int lua_json_encode__array( lua_State *L, luaL_Buffer *B, int index ) {
    int needSep = 0;

    luaL_addchar(B, '[');

    lua_pushnil(L);
    while ( lua_next(L, index) ) {
        if ( needSep ) {
            luaL_addchar(B, ',');
        }

        lua_json_encode__value(L, B, lua_absindex(L, -1));

        needSep = 1;

        lua_pop(L, 1);
    }

    luaL_addchar(B, ']');

    return 1;
}

static int lua_json_encode__object( lua_State *L, luaL_Buffer *B, int index ) {
    int needSep = 0;

    luaL_addchar(B, '{');

    lua_pushnil(L);
    while ( lua_next(L, index) ) {
        if ( needSep ) {
            luaL_addchar(B, ',');
        }

        lua_json_encode__value(L, B, lua_absindex(L, -2));

        luaL_addchar(B, ':');

        lua_json_encode__value(L, B, lua_absindex(L, -1));

        needSep = 1;

        lua_pop(L, 1);
    }

    luaL_addchar(B, '}');

    return 1;
}

static int lua_json_encode__string( lua_State *L, luaL_Buffer *B, int index ) {
    size_t len;
    const char *in = luaL_checklstring(L, index, &len);

    unsigned char c;
    size_t i = 0;

    luaL_addchar(B, '"');

    while ( i<len ) {
        c = in[i++];

        if ( c >= ' ' && c <= '~' && c != '\\' && c != '/' && c != '"' ) {
            luaL_addchar(B, c);
        } else {
            luaL_addchar(B, '\\');
            switch ( c ) {
                case '\\': luaL_addchar(B, '\\'); break;
                case '/':  luaL_addchar(B, '/'); break;
                case '"':  luaL_addchar(B, '"'); break;
                case '\a': luaL_addchar(B, 'a'); break;
                case '\b': luaL_addchar(B, 'b'); break;
                case '\f': luaL_addchar(B, 'f'); break;
                case '\n': luaL_addchar(B, 'n'); break;
                case '\r': luaL_addchar(B, 'r'); break;
                case '\t': luaL_addchar(B, 't'); break;
                case '\v': luaL_addchar(B, 'v'); break;
                default:
                    luaL_addchar(B, 'x');
                    luaL_addchar(B, hexdig[c >> 4]);
                    luaL_addchar(B, hexdig[c & 0xF]);
                    break;
            }
        }
    }

    luaL_addchar(B, '"');

    return 1;
}

static int lua_json_decode( lua_State *L ) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    size_t pos = 0;

    int r = lua_json_decode__scan_value(L, str, len, &pos);

    if ( r == 3 ) { // error
        return 3;
    } else { // ok
        while ( pos < len ) {
            if ( isspace(str[pos]) ) {
                pos++;
            } else {
                lua_fail_f(L, "garbage symbol '%c' at pos %d", 0, str[pos], pos);
            }
        }

        return 1; // ok
    }
}

static int lua_json_decode__scan_string( lua_State *L, const char *str, size_t len, size_t *pos ) {
    unsigned char c;
    unsigned int u;

    if ( str[*pos] == '"' ) { // empty string shortcut
        lua_pushstring(L, "");
        (*pos)++;
        return 1;
    }

    int strstart = (*pos) - 1; // start of opening quote (for error messages)
    int chunkstart = *pos; // for luaL_addlstring begin of common bytes
    int escaped = 0;

    luaL_Buffer B;
    luaL_buffinit(L, &B);

    while ( *pos < len ) {
        c = str[*pos];

        if ( escaped ) {
            if ( c == '"' || c == '\\' || c == '/' ) {
                luaL_addchar(&B, c);
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'a' ) {
                luaL_addchar(&B, '\a');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'b' ) {
                luaL_addchar(&B, '\b');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'f' ) {
                luaL_addchar(&B, '\f');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'n' ) {
                luaL_addchar(&B, '\n');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'r' ) {
                luaL_addchar(&B, '\r');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 't' ) {
                luaL_addchar(&B, '\t');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'v' ) {
                luaL_addchar(&B, '\v');
                (*pos)++;
                chunkstart = *pos;
                escaped = 0;
            } else if ( c == 'u' ) {
                if ( (*pos)+4 < len
                    && isxdigit(str[(*pos)+1])
                    && isxdigit(str[(*pos)+2])
                    && isxdigit(str[(*pos)+3])
                    && isxdigit(str[(*pos)+4])
                ) {
                    luaL_addlstring(&B, &(str[(*pos)-1]), 6);
                    (*pos) += 5;
                    chunkstart = *pos;
                    escaped = 0;
                } else {
                    lua_fail_f(L, "unexpected char at escaped sequence \\xXX, got '%c' at pos %d", 0, c, *pos);
                }
            } else if ( c == 'x' ) {
                if ( (*pos)+2 < len
                    && isxdigit(str[(*pos)+1])
                    && isxdigit(str[(*pos)+2])
                    && sscanf(&(str[(*pos)+1]), "%2x", &u) > 0
                ) {
                    luaL_addchar(&B, u);
                    //luaL_addlstring(&B, &(str[(*pos)-1]), 4);
                    (*pos) += 3;
                    chunkstart = *pos;
                    escaped = 0;
                } else {
                    lua_fail_f(L, "unexpected char at escaped sequence \\xXX, got \\x%c%c at pos %d", 0, str[(*pos)+1], str[(*pos)+2], *pos);
                }
            } else {
                lua_fail_f(L, "unexpected char at escaped sequence, got '%c' at pos %d", 0, c, *pos);
            }
        } else if ( c == '\\' ) { // start escape sequence
            if ( chunkstart && *pos > chunkstart ) {
                luaL_addlstring(&B, &(str[chunkstart]), (*pos)-chunkstart);
                chunkstart = 0;
            }
            escaped = *pos;
            (*pos)++;
        } else if ( c == '"' ) { // end of string
            if ( chunkstart && *pos > chunkstart ) {
                luaL_addlstring(&B, &(str[chunkstart]), (*pos)-chunkstart);
                chunkstart = 0;
            }
            luaL_pushresult(&B);
            (*pos)++;
            return 1;
        } else { // common symbol
            (*pos)++;
        }
    }

    if ( escaped ) {
        lua_fail_f(L, "unfinished escape sequence at pos %d for string at pos %d", 0, escaped, strstart);
    } else {
        lua_fail_f(L, "unfinished string at pos %d", 0, strstart);
    }
}

static int lua_json_decode__scan_array( lua_State *L, const char *str, size_t len, size_t *pos ) {
    unsigned char c;
    int start = (*pos) - 1; // start of opening bracket (for error messages)
    int r; // result
    int next_idx = 1;
    int state = 1; // 1 - need value, 0 - need sep

    while ( *pos < len ) {
        c = str[*pos];

        if ( isspace(c) ) { // skip space
            (*pos)++;
        } else if ( c == ']' ) { // end of array
            (*pos)++;
            return 1;
        } else if ( state ) { // look for value
            r = lua_json_decode__scan_value(L, str, len, pos);

            if ( r == 3 ) {
                return 3;
            } else {
                lua_seti(L, -2, next_idx++);
                state = 0;
            }
        } else if ( c == ',' ) { // look for sep
            (*pos)++;
            state = 1;
        } else {
            lua_fail_f(L, "unexpected symbol '%c' in array at pos %d", 0, c, *pos);
        }
    }

    lua_fail_f(L, "unfinished array at pos %d", 0, start);
}

static int lua_json_decode__scan_object( lua_State *L, const char *str, size_t len, size_t *pos ) {
    unsigned char c;
    int start = (*pos) - 1; // start of opening bracket (for error messages)
    int r; // result
    int state = 0; // 0 - need key (string), 1 - need semicolon (:), 2 - need value (any type), 3 - need sep (,)

    while ( *pos < len ) {
        c = str[*pos];

        if ( isspace(c) ) { // skip space
            (*pos)++;
        } else if ( state == 0 ) { // string or }
            if ( c == '"' ) { // string
                (*pos)++;
                r = lua_json_decode__scan_string(L, str, len, pos);

                if ( r == 3 ) {
                    return 3;
                } else {
                    state = 1;
                }
            } else if ( c == '}' ) { // end of object
                (*pos)++;
                return 1;
            } else {
                lua_fail_f(L, "unexpected symbol '%c' in object at pos %d", 0, c, *pos);
            }
        } else if ( state == 1 ) { // :
            if ( c == ':' ) {
                (*pos)++;
                state = 2;
            } else {
                lua_fail_f(L, "unexpected symbol '%c' in object at pos %d", 0, c, *pos);
            }
        } else if ( state == 2 ) { // value
            r = lua_json_decode__scan_value(L, str, len, pos);

            if ( r == 3 ) {
                return 3;
            } else {
                lua_settable(L, -3);
                state = 3;
            }
        } else { // , or }
            if ( c == ',' ) { // sep
                (*pos)++;
                state = 0;
            } else if ( c == '}' ) { // end of object
                (*pos)++;
                return 1;
            } else {
                lua_fail_f(L, "unexpected symbol '%c' in object at pos %d", 0, c, *pos);
            }
        }
    }

    lua_fail_f(L, "unfinished object at pos %d", 0, start);
}

static int lua_json_decode__scan_value( lua_State *L, const char *str, size_t len, size_t *pos ) {
    unsigned char c;
    double lf; // long float
    int r; // result
    int offset;

    while ( *pos < len ) {
        c = str[*pos];

        if ( isspace(c) ) { // skip space
            (*pos)++;
        } else if ( c == '{' && (*pos)+1 < len ) { // object
            if ( !lua_checkstack(L, 3) ) {
                lua_fail_f(L, "stack is full, pos %d", 0, *pos);
            }
            lua_newtable(L);
            (*pos)++;
            return lua_json_decode__scan_object(L, str, len, pos);
        } else if ( c == '[' && (*pos)+1 < len ) { // array
            if ( !lua_checkstack(L, 3) ) {
                lua_fail_f(L, "stack is full, pos %d", 0, *pos);
            }
            lua_newtable(L);
            (*pos)++;
            return lua_json_decode__scan_array(L, str, len, pos);
        } else if ( c == '"' && (*pos)+1 < len ) { // string
            (*pos)++;
            return lua_json_decode__scan_string(L, str, len, pos);
        } else if ( c == 'n' && (*pos)+3 < len
            && str[(*pos)+1]=='u'
            && str[(*pos)+2]=='l'
            && str[(*pos)+3]=='l'
        ) { // null
            lua_pushnil(L);
            (*pos) += 4;
            return 1;
        } else if ( isdigit(c)
            || c == '.'
            || c == '+' || c == '-'
            || c == 'N' || c == 'n' // NaN, nan
            || c == 'I' || c == 'i' // Infinity, inf
        ) { // number
            r = sscanf(&(str[*pos]), "%lf%n", &lf, &offset);

            if ( r > 0 ) {
                lua_pushnumber(L, lf);
                (*pos) += offset;
                return 1;
            } else {
                lua_fail_f(L, "bad number at pos %d", 0, *pos);
            }
        } else if ( c == 't' && (*pos)+3 < len
            && str[(*pos)+1]=='r'
            && str[(*pos)+2]=='u'
            && str[(*pos)+3]=='e'
        ) { // true
            lua_pushboolean(L, 1);
            (*pos) += 4;
            return 1;
        } else if ( c == 'f' && (*pos)+4 < len
            && str[(*pos)+1]=='a'
            && str[(*pos)+2]=='l'
            && str[(*pos)+3]=='s'
            && str[(*pos)+4]=='e'
        ) { // false
            lua_pushboolean(L, 0);
            (*pos) += 5;
            return 1;
        } else {
            lua_fail_f(L, "unexpected symbol '%c' at pos %d", 0, c, *pos);
        }
    }

    lua_fail(L, "unexpected end", 0);
}
