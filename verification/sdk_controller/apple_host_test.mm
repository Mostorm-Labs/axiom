#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import "AxiomCanvasView.h"
#include <axiom/native.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>

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
 [view detach];view.sessionToken=nil;view.sessionToken=@(token.c_str());[view layout];
 count=nlohmann::json::parse(call(R"({"command":"state"})"));if(count.at("value").at("objects")!=1||!count.at("value").at("canUndo"))throw std::runtime_error("remount lost document/history");
 if(axiom_native_close(token.c_str())!=0)throw std::runtime_error("close");view.sessionToken=nil;[window orderOut:nil];
 std::cout<<"AppKit native Host + real Metal/Runtime: input, independent surfaces, detach/remount History PASS (not RN qualification)\n";
}}
