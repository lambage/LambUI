
#ifndef LAMBUI_LUA_API_H
#define LAMBUI_LUA_API_H

#ifdef LAMBUI_LUA_STATIC_DEFINE
#  define LAMBUI_LUA_API
#  define LAMBUI_LUA_NO_EXPORT
#else
#  ifndef LAMBUI_LUA_API
#    ifdef lambui_lua_EXPORTS
        /* We are building this library */
#      define LAMBUI_LUA_API 
#    else
        /* We are using this library */
#      define LAMBUI_LUA_API 
#    endif
#  endif

#  ifndef LAMBUI_LUA_NO_EXPORT
#    define LAMBUI_LUA_NO_EXPORT 
#  endif
#endif

#ifndef LAMBUI_LUA_DEPRECATED
#  define LAMBUI_LUA_DEPRECATED __declspec(deprecated)
#endif

#ifndef LAMBUI_LUA_DEPRECATED_EXPORT
#  define LAMBUI_LUA_DEPRECATED_EXPORT LAMBUI_LUA_API LAMBUI_LUA_DEPRECATED
#endif

#ifndef LAMBUI_LUA_DEPRECATED_NO_EXPORT
#  define LAMBUI_LUA_DEPRECATED_NO_EXPORT LAMBUI_LUA_NO_EXPORT LAMBUI_LUA_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef LAMBUI_LUA_NO_DEPRECATED
#    define LAMBUI_LUA_NO_DEPRECATED
#  endif
#endif

#endif /* LAMBUI_LUA_API_H */
