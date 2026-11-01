#import "AxiomCanvasView.h"
#import "AxiomNative.h"
#import <QuartzCore/CAMetalLayer.h>

#include <map>
#include <vector>

@implementation AxiomCanvasHost{
 CAMetalLayer* _canonical;
 CAMetalLayer* _preview;
 NSTimer* _timer;
 std::uint64_t _attachment;
 CGSize _size;
 CGFloat _scale;
#if TARGET_OS_OSX
 BOOL _mouse;
#else
 std::map<UITouch*,std::uint64_t> _touches;
 std::uint64_t _nextTouch;
#endif
}
#if TARGET_OS_OSX
- (BOOL)isFlipped{return YES;}
- (BOOL)acceptsFirstResponder{return YES;}
#endif
- (instancetype)initWithFrame:(CGRect)frame{
 if((self=[super initWithFrame:frame])){
#if TARGET_OS_OSX
  self.wantsLayer=YES;
#else
  self.multipleTouchEnabled=YES;
#endif
  _canonical=[CAMetalLayer layer];_preview=[CAMetalLayer layer];_preview.opaque=NO;
  [self.layer addSublayer:_canonical];[self.layer addSublayer:_preview];
  __weak AxiomCanvasHost* weak=self;
  _timer=[NSTimer scheduledTimerWithTimeInterval:1.0/60 repeats:YES block:^(NSTimer*){[weak serviceFrame];}];
 }return self;
}
- (void)dealloc{[_timer invalidate];[self detach];}
- (void)setSessionToken:(NSString*)token{if([_sessionToken isEqualToString:token])return;[self detach];_sessionToken=[token copy];[self bind];}
- (void)detach{if(_attachment&&_sessionToken)axiom_native_detach(_sessionToken.UTF8String,_attachment);_attachment=0;_preview.hidden=YES;}
- (void)bind{
 if(!_sessionToken.length||self.bounds.size.width<=0||self.bounds.size.height<=0)return;
#if TARGET_OS_OSX
 const auto scale=self.window.backingScaleFactor?:NSScreen.mainScreen.backingScaleFactor;
#else
 const auto scale=self.window.screen.scale?:UIScreen.mainScreen.scale;
#endif
 _canonical.frame=_preview.frame=self.bounds;_canonical.contentsScale=_preview.contentsScale=scale;
 if(!_attachment){axiom_native_view view{sizeof(view),1,(__bridge void*)_canonical,(__bridge void*)_preview,float(self.bounds.size.width),float(self.bounds.size.height),float(scale)};(void)axiom_native_attach(_sessionToken.UTF8String,&view,&_attachment);}
 else if(!CGSizeEqualToSize(_size,self.bounds.size)||_scale!=scale)(void)axiom_native_resize(_sessionToken.UTF8String,_attachment,float(self.bounds.size.width),float(self.bounds.size.height),float(scale));
 _size=self.bounds.size;_scale=scale;
}
#if TARGET_OS_OSX
- (void)layout{[super layout];[self bind];}
- (void)viewDidMoveToWindow{[super viewDidMoveToWindow];if(self.window)[self bind];else[self detach];}
#else
- (void)layoutSubviews{[super layoutSubviews];[self bind];}
- (void)didMoveToWindow{[super didMoveToWindow];if(self.window)[self bind];else[self detach];}
#endif
- (void)serviceFrame{if(_attachment&&_sessionToken)(void)axiom_native_frame(_sessionToken.UTF8String,_attachment);}
#if TARGET_OS_OSX
- (void)sendMouse:(NSEvent*)event phase:(unsigned)phase{
 if(!_attachment)return;const auto p=[self convertPoint:event.locationInWindow fromView:nil];
 axiom_native_sample s{1,1,std::uint64_t(event.timestamp*1e9),float(p.x),float(p.y),0,0,0,phase,0,0,0};
 (void)axiom_native_input(_sessionToken.UTF8String,_attachment,&s,1);
}
- (void)mouseDown:(NSEvent*)event{[self.window makeFirstResponder:self];_mouse=YES;[self sendMouse:event phase:0];}
- (void)mouseDragged:(NSEvent*)event{if(_mouse)[self sendMouse:event phase:1];}
- (void)mouseUp:(NSEvent*)event{if(_mouse)[self sendMouse:event phase:2];_mouse=NO;}
#else
- (void)sendTouches:(NSSet<UITouch*>*)touches event:(UIEvent*)event phase:(unsigned)phase{
 if(!_attachment)return;
 for(UITouch* touch in touches){
  if(phase==0)_touches[touch]=++_nextTouch;
  const auto it=_touches.find(touch);if(it==_touches.end())continue;
  NSArray<UITouch*>* samples=phase==1?[event coalescedTouchesForTouch:touch]:@[touch];
  std::vector<axiom_native_sample> batch;
  for(UITouch* t in samples){const auto p=[t locationInView:self];const bool pressure=t.type==UITouchTypePencil&&t.maximumPossibleForce>0;
   batch.push_back({2,it->second,std::uint64_t(t.timestamp*1e9),float(p.x),float(p.y),pressure?float(t.force/t.maximumPossibleForce):0,0,0,phase,t.type==UITouchTypePencil?2U:1U,pressure?1U:0U,0});}
  (void)axiom_native_input(_sessionToken.UTF8String,_attachment,batch.data(),batch.size());
  if(phase==2||phase==3)_touches.erase(it);
 }
}
- (void)touchesBegan:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e{[self sendTouches:t event:e phase:0];}
- (void)touchesMoved:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e{[self sendTouches:t event:e phase:1];}
- (void)touchesEnded:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e{[self sendTouches:t event:e phase:2];}
- (void)touchesCancelled:(NSSet<UITouch*>*)t withEvent:(UIEvent*)e{[self sendTouches:t event:e phase:3];}
#endif
@end
