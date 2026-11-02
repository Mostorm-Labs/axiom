#import "AxiomNative.h"
#import <AxiomNativeSpec/AxiomNativeSpec.h>
#include <vector>
#include <cmath>

BOOL AxiomInitializeNative(void){
 NSCAssert(NSThread.isMainThread,@"Axiom owner thread");
 NSString* bundle=[[NSBundle bundleForClass:NSClassFromString(@"AxiomNative")] pathForResource:@"AxiomAssets" ofType:@"bundle"];
 if(!bundle)return NO;
 const char* path=bundle.UTF8String;return axiom_native_initialize(path,strlen(path))==0;
}
static std::vector<std::uint8_t> bytes(NSArray* value){
 if(value.count>134217728)throw std::invalid_argument("bytes limit");
 std::vector<std::uint8_t> result;result.reserve(value.count);
 for(NSNumber* n in value){const double v=n.doubleValue;if(v<0||v>255||v!=std::floor(v))throw std::invalid_argument("byte");result.push_back(static_cast<std::uint8_t>(v));}return result;
}
@interface AxiomNative:NSObject<NativeAxiomSpec>
@end
@implementation AxiomNative
RCT_EXPORT_MODULE(AxiomNative)
+ (BOOL)requiresMainQueueSetup{return YES;}
- (dispatch_queue_t)methodQueue{return dispatch_get_main_queue();}
- (void)createSession:(NSString*)options snapshot:(NSArray*)snapshot resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject{
 options=[options copy];snapshot=[snapshot copy];
 dispatch_async(dispatch_get_main_queue(), ^{try{if(!AxiomInitializeNative())throw std::runtime_error("Missing matching AxiomAssets bundle");const auto data=bytes(snapshot);const char* json=options.UTF8String;char token[33]{};if(axiom_native_create(json,strlen(json),data.data(),data.size(),token))throw std::runtime_error("create rejected");resolve(@(token));}catch(const std::exception& e){reject(@"CREATE_REJECTED",@(e.what()),nil);}});
}
- (void)request:(NSString*)session request:(NSString*)request bytes:(NSArray*)value resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject{
 session=[session copy];request=[request copy];value=[value copy];
 dispatch_async(dispatch_get_main_queue(), ^{try{const auto data=bytes(value);const char* json=request.UTF8String;axiom_native_buffer buffer=0;
  if(axiom_native_call(session.UTF8String,json,strlen(json),data.data(),data.size(),&buffer))throw std::runtime_error("transport rejected");
  struct Release{axiom_native_buffer buffer;~Release(){axiom_native_buffer_release(buffer);}}release{buffer};size_t n=0;axiom_native_buffer_copy(buffer,nullptr,0,&n);std::string reply(n,'\0');if(axiom_native_buffer_copy(buffer,reply.data(),n,&n))throw std::runtime_error("buffer copy");resolve(@(reply.c_str()));
 }catch(const std::exception& e){reject(@"TRANSPORT_REJECTED",@(e.what()),nil);}});
}
- (void)close:(NSString*)session resolve:(RCTPromiseResolveBlock)resolve reject:(RCTPromiseRejectBlock)reject{
 session=[session copy];dispatch_async(dispatch_get_main_queue(), ^{if(axiom_native_close(session.UTF8String)==0)resolve(nil);else reject(@"CLOSE_REJECTED",@"Finish IME or drain data custody, then retry",nil);});
}
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const facebook::react::ObjCTurboModule::InitParams&)params{return std::make_shared<facebook::react::NativeAxiomSpecJSI>(params);}
@end
