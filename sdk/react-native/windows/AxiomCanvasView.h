#pragma once
#include "AxiomCanvasView.g.h"
#include <ComponentView.Experimental.interop.h>
#include <axiom/native.h>
#include <windowsx.h>
#include <cmath>
#include <string>
#include <vector>
namespace MostormAxiom {
class AxiomCanvasView:public winrt::implements<AxiomCanvasView,winrt::Windows::Foundation::IInspectable>,public BaseAxiomCanvasView<AxiomCanvasView>{
 HWND parent_=nullptr,window_=nullptr;
 std::string token_;
 std::uint64_t generation_=0;
 float width_=0,height_=0,dpr_=1;
 static LRESULT CALLBACK proc(HWND window,UINT message,WPARAM w,LPARAM l){
  auto* self=reinterpret_cast<AxiomCanvasView*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  if(message==WM_NCCREATE){self=static_cast<AxiomCanvasView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
  if(self){try{
   if(message==WM_TIMER){if(self->generation_)axiom_native_frame(self->token_.c_str(),self->generation_);return 0;}
   if(message==WM_ERASEBKGND)return 1;
   if(message==WM_POINTERDOWN||message==WM_POINTERUPDATE||message==WM_POINTERUP){
    const auto pointer=GET_POINTERID_WPARAM(w);POINTER_INPUT_TYPE type=PT_POINTER;GetPointerType(pointer,&type);
    std::vector<axiom_native_sample> samples;
    auto sample=[&](const POINTER_INFO& info,float pressure,bool hasPressure){POINT p=info.ptPixelLocation;ScreenToClient(window,&p);LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);const auto time=info.PerformanceCount?std::uint64_t((static_cast<long double>(info.PerformanceCount)*1e9L)/frequency.QuadPart):GetTickCount64()*1000000;
      samples.push_back({type==PT_PEN?3ULL:2ULL,pointer,time,p.x/self->dpr_,p.y/self->dpr_,pressure,0,0,message==WM_POINTERDOWN?0U:message==WM_POINTERUP?2U:1U,type==PT_PEN?2U:1U,hasPressure?1U:0U,0});};
    if(type==PT_PEN){UINT32 count=0;GetPointerPenInfoHistory(pointer,&count,nullptr);if(!count||count>4096)return 0;std::vector<POINTER_PEN_INFO> history(count);if(!GetPointerPenInfoHistory(pointer,&count,history.data()))return 0;for(auto it=history.rbegin();it!=history.rend();++it)sample(it->pointerInfo,it->pressure/1024.0F,(it->penMask&PEN_MASK_PRESSURE)!=0);}
    else{POINTER_INFO info{};if(!GetPointerInfo(pointer,&info))return 0;sample(info,0,false);}
    if(self->generation_)axiom_native_input(self->token_.c_str(),self->generation_,samples.data(),samples.size());return 0;
   }
   if(message==WM_LBUTTONDOWN||message==WM_MOUSEMOVE||message==WM_LBUTTONUP){
    // Promoted WM_POINTER mouse packets are not a second input sequence.
    if((GetMessageExtraInfo()&0xFFFFFF80)==0xFF515700)return 0;
    if(message==WM_MOUSEMOVE&&!(w&MK_LBUTTON))return 0;
    if(message==WM_LBUTTONDOWN)SetCapture(window);
    axiom_native_sample s{1,1,GetTickCount64()*1000000,GET_X_LPARAM(l)/self->dpr_,GET_Y_LPARAM(l)/self->dpr_,0,0,0,message==WM_LBUTTONDOWN?0U:message==WM_LBUTTONUP?2U:1U,0,0,0};
    if(self->generation_)axiom_native_input(self->token_.c_str(),self->generation_,&s,1);
    if(message==WM_LBUTTONUP)ReleaseCapture();return 0;
   }
   if(message==WM_KILLFOCUS||message==WM_CANCELMODE){self->detach();self->attach();}
  }catch(...){return 0;}}
  return DefWindowProcW(window,message,w,l);
 }
 void detach()noexcept{if(generation_&&!token_.empty())axiom_native_detach(token_.c_str(),generation_);generation_=0;}
 void attach()noexcept{if(generation_||token_.empty()||!window_||width_<=0||height_<=0)return;axiom_native_view view{sizeof(view),1,window_,this,width_,height_,dpr_};axiom_native_attach(token_.c_str(),&view,&generation_);}
public:
 ~AxiomCanvasView(){detach();if(window_){KillTimer(window_,1);DestroyWindow(window_);}}
 void Initialize(const winrt::Microsoft::ReactNative::ComponentView& view)noexcept{
  try{auto interop=view.as<::Microsoft::ReactNative::Composition::Experimental::IComponentViewInterop>();parent_=interop->GetHwndForParenting();}catch(...){parent_=nullptr;}
  static const auto registered=[](){WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"AxiomRNCanvasV1";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);return RegisterClassW(&c);}();(void)registered;
 }
 winrt::Microsoft::UI::Composition::Visual CreateVisual(const winrt::Microsoft::ReactNative::ComponentView& view)noexcept{auto v=view.as<winrt::Microsoft::ReactNative::Composition::ComponentView>().Compositor().CreateSpriteVisual();v.Opacity(0);return v;}
 void UpdateProps(const winrt::Microsoft::ReactNative::ComponentView&,const winrt::com_ptr<AxiomCanvasViewProps>& props,const winrt::com_ptr<AxiomCanvasViewProps>&)noexcept{
  const auto& token=props->sessionToken;if(token!=token_){detach();token_=token;attach();}
 }
 void UpdateLayoutMetrics(const winrt::Microsoft::ReactNative::ComponentView& view,const winrt::Microsoft::ReactNative::LayoutMetrics& metrics,const winrt::Microsoft::ReactNative::LayoutMetrics&)noexcept{
  try{if(!parent_)return;float x=metrics.Frame.X,y=metrics.Frame.Y;auto parent=view.Parent();while(parent){x+=parent.LayoutMetrics().Frame.X;y+=parent.LayoutMetrics().Frame.Y;parent=parent.Parent();}
   width_=metrics.Frame.Width;height_=metrics.Frame.Height;dpr_=metrics.PointScaleFactor>0?metrics.PointScaleFactor:1;
   if(!window_){window_=CreateWindowExW(0,L"AxiomRNCanvasV1",L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,1,1,parent_,nullptr,GetModuleHandleW(nullptr),this);if(window_)SetTimer(window_,1,16,nullptr);}
   // RNW's ContentIsland is a sibling HWND. Keep this bounded native canvas
   // above that sibling so the canvas, rather than RNW, receives its input.
   if(!window_)return;SetWindowPos(window_,HWND_TOP,int(std::lround(x*dpr_)),int(std::lround(y*dpr_)),int(std::lround(width_*dpr_)),int(std::lround(height_*dpr_)),SWP_NOACTIVATE|SWP_SHOWWINDOW);
   if(generation_)axiom_native_resize(token_.c_str(),generation_,width_,height_,dpr_);else attach();
  }catch(...){detach();}
 }
};
}
