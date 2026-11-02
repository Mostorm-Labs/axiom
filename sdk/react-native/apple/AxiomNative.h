#import <Foundation/Foundation.h>
#if __has_include(<AxiomNative/native.h>)
#include <AxiomNative/native.h>
#else
#include <axiom/native.h>
#endif
// Private platform initialization; all Session/View calls use the public C ABI.
BOOL AxiomInitializeNative(void);
