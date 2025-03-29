
#ifndef VULKAN_WRAPPER_EXPORT_H
#define VULKAN_WRAPPER_EXPORT_H

#ifdef VULKAN_WRAPPER_STATIC_DEFINE
#  define VULKAN_WRAPPER_EXPORT
#  define VULKAN_WRAPPER_NO_EXPORT
#else
#  ifndef VULKAN_WRAPPER_EXPORT
#    ifdef vulkan_wrapper_EXPORTS
        /* We are building this library */
#      define VULKAN_WRAPPER_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define VULKAN_WRAPPER_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef VULKAN_WRAPPER_NO_EXPORT
#    define VULKAN_WRAPPER_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef VULKAN_WRAPPER_DEPRECATED
#  define VULKAN_WRAPPER_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef VULKAN_WRAPPER_DEPRECATED_EXPORT
#  define VULKAN_WRAPPER_DEPRECATED_EXPORT VULKAN_WRAPPER_EXPORT VULKAN_WRAPPER_DEPRECATED
#endif

#ifndef VULKAN_WRAPPER_DEPRECATED_NO_EXPORT
#  define VULKAN_WRAPPER_DEPRECATED_NO_EXPORT VULKAN_WRAPPER_NO_EXPORT VULKAN_WRAPPER_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef VULKAN_WRAPPER_NO_DEPRECATED
#    define VULKAN_WRAPPER_NO_DEPRECATED
#  endif
#endif

#endif /* VULKAN_WRAPPER_EXPORT_H */
