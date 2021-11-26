// This file contains miscellaneous tricks that are handled by the C preprocessor
#define isDefined(thing) (typeof thing !== "undefined")
#define isFunction(thing) (typeof thing === "function")

#ifdef COMPILE_WEBUI // This block only required for build and not on client
#else
//For the client we divert to nop chaining function
#define setConditionsForCheck(...) getThis()
#define setRdb(...) getThis()
#define setUci(...) getThis()
#define setShell(...) getThis()
#define addExtraScript(...)
#endif

