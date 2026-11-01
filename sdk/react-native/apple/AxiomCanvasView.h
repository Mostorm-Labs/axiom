#import <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
@interface AxiomCanvasHost:NSView
#else
#import <UIKit/UIKit.h>
@interface AxiomCanvasHost:UIView
#endif
@property(nonatomic,copy)NSString* sessionToken;
- (void)detach;
@end
