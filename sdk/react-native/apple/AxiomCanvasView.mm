#import <TargetConditionals.h>
#import "AxiomCanvasView.h"
#if TARGET_OS_OSX
#import <React/RCTViewComponentView.h>
#import <react/renderer/components/AxiomNativeSpec/ComponentDescriptors.h>
#import <react/renderer/components/AxiomNativeSpec/Props.h>

@interface AxiomCanvasView : RCTViewComponentView
@end

@implementation AxiomCanvasView {
  AxiomCanvasHost* _host;
}

+ (facebook::react::ComponentDescriptorProvider)componentDescriptorProvider {
  return facebook::react::concreteComponentDescriptorProvider<
      facebook::react::AxiomCanvasViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    _host = [[AxiomCanvasHost alloc] initWithFrame:self.bounds];
    _host.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.contentView = _host;
  }
  return self;
}

- (void)updateProps:(const facebook::react::Props::Shared&)props
           oldProps:(const facebook::react::Props::Shared&)oldProps {
  [super updateProps:props oldProps:oldProps];
  const auto& value = *std::static_pointer_cast<const facebook::react::AxiomCanvasViewProps>(props);
  _host.sessionToken = @(value.sessionToken.c_str());
}

- (void)prepareForRecycle {
  [_host detach];
  _host.sessionToken = nil;
  [super prepareForRecycle];
}

@end

Class<RCTComponentViewProtocol> AxiomCanvasViewCls(void) {
  return AxiomCanvasView.class;
}
#else
#import <React/RCTViewComponentView.h>
#import <react/renderer/components/AxiomNativeSpec/ComponentDescriptors.h>
#import <react/renderer/components/AxiomNativeSpec/Props.h>
@interface AxiomCanvasView:RCTViewComponentView
@end
@implementation AxiomCanvasView{
 AxiomCanvasHost* _host;
}
+ (facebook::react::ComponentDescriptorProvider)componentDescriptorProvider{return facebook::react::concreteComponentDescriptorProvider<facebook::react::AxiomCanvasViewComponentDescriptor>();}
- (instancetype)initWithFrame:(CGRect)frame{if((self=[super initWithFrame:frame])){_host=[[AxiomCanvasHost alloc]initWithFrame:self.bounds];_host.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;self.contentView=_host;}return self;}
- (void)updateProps:(const facebook::react::Props::Shared&)props oldProps:(const facebook::react::Props::Shared&)oldProps{[super updateProps:props oldProps:oldProps];const auto& value=*std::static_pointer_cast<const facebook::react::AxiomCanvasViewProps>(props);_host.sessionToken=@(value.sessionToken.c_str());}
- (void)prepareForRecycle{[_host detach];_host.sessionToken=nil;[super prepareForRecycle];}
@end
Class<RCTComponentViewProtocol> AxiomCanvasViewCls(void){return AxiomCanvasView.class;}
#endif
