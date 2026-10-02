
#ifndef LAMBUI_API_H
#define LAMBUI_API_H

#ifdef LAMBUI_STATIC_DEFINE
#  define LAMBUI_API
#  define LAMBUI_NO_EXPORT
#else
#  ifndef LAMBUI_API
#    ifdef lambui_EXPORTS
        /* We are building this library */
#      define LAMBUI_API 
#    else
        /* We are using this library */
#      define LAMBUI_API 
#    endif
#  endif

#  ifndef LAMBUI_NO_EXPORT
#    define LAMBUI_NO_EXPORT 
#  endif
#endif

#ifndef LAMBUI_DEPRECATED
#  define LAMBUI_DEPRECATED __declspec(deprecated)
#endif

#ifndef LAMBUI_DEPRECATED_EXPORT
#  define LAMBUI_DEPRECATED_EXPORT LAMBUI_API LAMBUI_DEPRECATED
#endif

#ifndef LAMBUI_DEPRECATED_NO_EXPORT
#  define LAMBUI_DEPRECATED_NO_EXPORT LAMBUI_NO_EXPORT LAMBUI_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef LAMBUI_NO_DEPRECATED
#    define LAMBUI_NO_DEPRECATED
#  endif
#endif

#endif /* LAMBUI_API_H */
