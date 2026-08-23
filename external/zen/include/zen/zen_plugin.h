/* =========================================================
** zen_plugin.h — Public API header for Zen plugins (.so/.dll)
**
** A plugin is a shared library that exports:
**   extern "C" const zen::NativeLib* zen_open_<name>(void);
**
** The returned NativeLib must be static (lives forever).
** Plugin functions use the same signature as builtins:
**   int myfunc(zen::VM *vm, zen::Value *args, int nargs);
**
** Compile: g++ -shared -fPIC -o myplugin.so myplugin.cpp -I<zen_src>
** ========================================================= */

#ifndef ZEN_PLUGIN_H
#define ZEN_PLUGIN_H

#include "module.h"
#include "vm.h"

/* Helper macro for plugin export */
#ifdef _WIN32
#define ZEN_EXPORT extern "C" __declspec(dllexport)
#else
#define ZEN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

/*
** Usage in plugin:
**
**   #include "zen_plugin.h"
**   using namespace zen;
**
**   static int my_func(VM *vm, Value *args, int nargs) {
**       args[0] = val_int(42);
**       return 1;
**   }
**
**   static const NativeReg funcs[] = { {"answer", my_func, 0} };
**   static const NativeLib lib = { "myplugin", funcs, 1, nullptr, 0 };
**
**   ZEN_EXPORT const NativeLib* zen_open_myplugin(void) {
**       return &lib;
**   }
*/

#endif /* ZEN_PLUGIN_H */
