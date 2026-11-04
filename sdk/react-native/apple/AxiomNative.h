#import <Foundation/Foundation.h>
#if __has_include(<axiom/native.h>)
#include <axiom/native.h>
#elif __has_include(<AxiomNative/native.h>)
#include <AxiomNative/native.h>
#else
#include <native.h>
#endif
// Private platform initialization; all Session/View calls use the public C ABI.
BOOL AxiomInitializeNative(void);
