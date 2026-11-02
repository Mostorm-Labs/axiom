#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import "AxiomCanvasView.h"
#include <axiom/native.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <thread>
#include <limits>

// NSEvent has no public constructor for magnification. Supply only the OS
// event facts; the real host, C ABI, Runtime and Metal providers execute below.
@interface TrackpadEvent : NSEvent
@property(nonatomic) CGFloat testMagnification;
@property(nonatomic) CGFloat testDeltaX;
@property(nonatomic) CGFloat testDeltaY;
@property(nonatomic) NSPoint testLocation;
@property(nonatomic) NSEventPhase testPhase;
@end
@implementation TrackpadEvent
- (CGFloat)magnification { return _testMagnification; }
- (CGFloat)scrollingDeltaX { return _testDeltaX; }
- (CGFloat)scrollingDeltaY { return _testDeltaY; }
- (BOOL)hasPreciseScrollingDeltas { return YES; }
- (NSPoint)locationInWindow { return _testLocation; }
- (NSEventPhase)phase { return _testPhase; }
- (NSEventPhase)momentumPhase { return NSEventPhaseNone; }
@end

@interface AxiomCanvasHost (FrameServiceTest)
- (void)serviceFrame;
@end
@interface TrackingCanvasHost : AxiomCanvasHost
@property(nonatomic) NSUInteger servicedFrames;
@end
@implementation TrackingCanvasHost
- (void)serviceFrame { ++_servicedFrames; [super serviceFrame]; }
@end

int main(){@autoreleasepool{
 [NSApplication sharedApplication];
 const std::string root=NATIVE_TEST_ASSETS;axiom_native_initialize(root.c_str(),root.size());char session[33]{};
 const std::string options=R"({"documentId":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","builtinFonts":false})";
 if(axiom_native_create(options.c_str(),options.size(),nullptr,0,session))throw std::runtime_error("create");const std::string token=session;
 const auto call=[&](const std::string& json){axiom_native_buffer buffer=0;if(axiom_native_call(session,json.c_str(),json.size(),nullptr,0,&buffer))throw std::runtime_error("call");size_t n=0;axiom_native_buffer_copy(buffer,nullptr,0,&n);std::string result(n,'\0');axiom_native_buffer_copy(buffer,result.data(),n,&n);axiom_native_buffer_release(buffer);return result;};
 NSWindow* window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,512,512) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
 TrackingCanvasHost* view=[[TrackingCanvasHost alloc]initWithFrame:NSMakeRect(0,0,512,512)];window.contentView=view;[window orderFront:nil];view.sessionToken=@(token.c_str());[view layout];
 const auto state=nlohmann::json::parse(call(R"({"command":"state"})"));if(!state.value("ok",false)||!state.at("value").at("surface").at("available"))throw std::runtime_error(state.dump());
 for(unsigned i=0;i<3;++i){const float x=30+40*i,y=80;
 // AppKit collection exercised below by real NSEvent, not JS pointer command.
 NSEventType type=i==0?NSEventTypeLeftMouseDown:i==2?NSEventTypeLeftMouseUp:NSEventTypeLeftMouseDragged;
 NSEvent* event=[NSEvent mouseEventWithType:type location:NSMakePoint(x,512-y) modifierFlags:0 timestamp:double(i+1)*.01 windowNumber:window.windowNumber context:nil eventNumber:i clickCount:1 pressure:.5];
 if(i==0)[view mouseDown:event];else if(i==1)[view mouseDragged:event];else[view mouseUp:event];
 if(i==1){
  // AppKit services a drag in event-tracking mode. A timer registered only in
  // the default mode cannot present the preview until after mouse-up.
  const auto frames=view.servicedFrames;
  const auto deadline=[NSDate dateWithTimeIntervalSinceNow:1.0];
  while(deadline.timeIntervalSinceNow>0)
   [NSRunLoop.currentRunLoop runMode:NSEventTrackingRunLoopMode beforeDate:deadline];
  if(view.servicedFrames<=frames)throw std::runtime_error("frame service paused during AppKit event tracking");
  auto* overlay=(CAMetalLayer*)view.layer.sublayers.lastObject;
  if(overlay.hidden)throw std::runtime_error("preview did not present during AppKit event tracking");
  const auto dragging=nlohmann::json::parse(call(R"({"command":"state"})"));
  if(dragging.at("value").at("objects")!=0)throw std::runtime_error("preview committed before mouse-up");
 }
 }
 for(unsigned i=0;i<20;++i)[NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.02]];
 auto count=nlohmann::json::parse(call(R"({"command":"state"})"));if(count.at("value").at("objects")!=1)throw std::runtime_error(count.dump());
 const auto beforeGesture=count.at("value");
 TrackpadEvent* pinch=[TrackpadEvent new];
 pinch.testLocation=NSMakePoint(100,392); // flipped view anchor (100,120)
 pinch.testMagnification=.5;pinch.testPhase=NSEventPhaseBegan;
 [view magnifyWithEvent:pinch];
 auto camera=nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera");
 if(std::abs(camera.at("scale").get<double>()-1.5)>1e-5 ||
    std::abs(camera.at("x").get<double>()+50)>1e-4 ||
    std::abs(camera.at("y").get<double>()+60)>1e-4)
   throw std::runtime_error("AppKit pinch did not preserve the view-local anchor: "+camera.dump());
 pinch.testMagnification=0;pinch.testPhase=NSEventPhaseEnded;[view magnifyWithEvent:pinch];
 pinch.testMagnification=-.2;pinch.testPhase=NSEventPhaseBegan;[view magnifyWithEvent:pinch];
 TrackpadEvent* scroll=[TrackpadEvent new];scroll.testDeltaX=12;scroll.testDeltaY=-8;
 scroll.testPhase=NSEventPhaseChanged;[view scrollWheel:scroll];
 camera=nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera");
 if(std::abs(camera.at("scale").get<double>()-1.2)>1e-5 ||
    std::abs(camera.at("x").get<double>()+8)>1e-4 ||
    std::abs(camera.at("y").get<double>()+32)>1e-4)
   throw std::runtime_error("AppKit repeated pinch/pan direction: "+camera.dump());
 pinch.testMagnification=0;pinch.testPhase=NSEventPhaseCancelled;[view magnifyWithEvent:pinch];
 const auto afterGesture=nlohmann::json::parse(call(R"({"command":"state"})")).at("value");
 for(const auto* field:{"objects","generation","canUndo","canRedo","activeContacts"})
   if(afterGesture.at(field)!=beforeGesture.at(field))throw std::runtime_error("gesture mutated document/History");
 if(afterGesture.at("camera")!=camera)throw std::runtime_error("terminal gesture changed camera");
 // Exercise the exported C boundary, not a JS command or unversioned host call.
 const auto attachment=[[view valueForKey:@"attachment"] unsignedLongLongValue];
 axiom_native_navigation_sample navigation{sizeof(navigation),AXIOM_NATIVE_ABI_VERSION,
   AXIOM_NATIVE_ZOOM,0,0,0,0,2};
 const auto unchanged=[&]{
   if(nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera")!=camera)
     throw std::runtime_error("rejected navigation mutated camera");
 };
 if(axiom_native_navigation(token.c_str(),attachment+1,&navigation)!=AXIOM_NATIVE_REJECTED)
   throw std::runtime_error("stale navigation admitted");unchanged();
 const std::string foreignToken(32,'c');
 if(axiom_native_navigation(foreignToken.c_str(),attachment,&navigation)!=AXIOM_NATIVE_REJECTED)
   throw std::runtime_error("foreign navigation admitted");unchanged();
 int threadResult=0;std::thread other([&]{threadResult=axiom_native_navigation(token.c_str(),attachment,&navigation);});other.join();
 if(threadResult!=AXIOM_NATIVE_WRONG_THREAD)throw std::runtime_error("navigation wrong thread admitted");unchanged();
 const auto reject=[&](axiom_native_navigation_sample invalid){
   if(axiom_native_navigation(token.c_str(),attachment,&invalid)!=AXIOM_NATIVE_INVALID_ARGUMENT)
     throw std::runtime_error("malformed navigation admitted");unchanged();
 };
 auto invalid=navigation;invalid.size=0;reject(invalid);
 invalid=navigation;invalid.abi_version=2;reject(invalid);
 invalid=navigation;invalid.kind=99;reject(invalid);
 invalid=navigation;invalid.scale_delta=0;reject(invalid);
 invalid=navigation;invalid.scale_delta=-1;reject(invalid);
 invalid=navigation;invalid.anchor_x=std::numeric_limits<float>::quiet_NaN();reject(invalid);
 invalid=navigation;invalid.delta_y=std::numeric_limits<float>::infinity();reject(invalid);
 // Zero is a real anchor (the control toolbar's optional-anchor fallback must
 // not replace it with view center). At 2x relative zoom: (-8,-32)->(-16,-64).
 if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=AXIOM_NATIVE_OK)
   throw std::runtime_error("zero-anchor navigation rejected");
 auto zero=nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera");
 if(std::abs(zero.at("scale").get<double>()-2.4)>1e-5 ||
    std::abs(zero.at("x").get<double>()+16)>1e-4 ||
    std::abs(zero.at("y").get<double>()+64)>1e-4)throw std::runtime_error("zero anchor changed");
 navigation.scale_delta=100;if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=0)throw std::runtime_error("zoom limit");
 if(nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera").at("scale")!=8.0)throw std::runtime_error("max zoom");
 navigation.scale_delta=.0001F;if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=0)throw std::runtime_error("zoom limit");
 if(nlohmann::json::parse(call(R"({"command":"state"})")).at("value").at("camera").at("scale")!=.25)throw std::runtime_error("min zoom");
 [view detach];
 if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=AXIOM_NATIVE_REJECTED)
   throw std::runtime_error("detached navigation admitted");
 view.sessionToken=nil;view.sessionToken=@(token.c_str());[view layout];
 if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=AXIOM_NATIVE_REJECTED)
   throw std::runtime_error("old attachment navigation after remount admitted");
 count=nlohmann::json::parse(call(R"({"command":"state"})"));if(count.at("value").at("objects")!=1||!count.at("value").at("canUndo"))throw std::runtime_error("remount lost document/history");
 if(axiom_native_close(token.c_str())!=0)throw std::runtime_error("close");view.sessionToken=nil;[window orderOut:nil];
 if(axiom_native_navigation(token.c_str(),attachment,&navigation)!=AXIOM_NATIVE_REJECTED)
   throw std::runtime_error("closed session navigation admitted");
 std::cout<<"AppKit host/Metal: pointer preview, pinch anchor, repeated zoom, scroll direction, terminal camera, limits, C ABI rejection and remount History PASS (synthetic events, not physical trackpad qualification)\n";
}}
