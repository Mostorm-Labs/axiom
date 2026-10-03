#define NOMINMAX
#include "canvas/debug_ui/windows_host.hpp"
#if defined(_WIN32)
#include "imgui.h"
#include <algorithm>
#include <array>
#include <sstream>
namespace canvas::debug_ui {
namespace { constexpr wchar_t kDebugUiClass[] = L"AxiomDebugUiPanel"; constexpr int kPanelWidth=410; constexpr int kPanelHeight=560; void fill(HDC dc,int l,int t,int r,int b,COLORREF c){RECT q{l,t,r,b};HBRUSH x=CreateSolidBrush(c);FillRect(dc,&q,x);DeleteObject(x);} void text(HDC dc,int x,int y,const std::wstring& v,COLORREF c=RGB(225,230,238)){SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);TextOutW(dc,x,y,v.c_str(),static_cast<int>(v.size()));} std::wstring stateName(CapabilityState s){switch(s){case CapabilityState::kAvailable:return L"Available";case CapabilityState::kDegraded:return L"Degraded";default:return L"Unavailable";}} }
WindowsDebugUiHost::~WindowsDebugUiHost(){shutdown();}
bool WindowsDebugUiHost::initialize(HWND w){
  if(initialized_||!w)return initialized_;
  window_=w;
  HINSTANCE i=reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(w,GWLP_HINSTANCE));
  WNDCLASSW k{};
  k.hInstance=i;
  k.lpfnWndProc=overlayWindowProc;
  k.lpszClassName=kDebugUiClass;
  k.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
  k.hbrBackground=static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
  if(RegisterClassW(&k)==0&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
  // The preview is an owned popup backed by DirectComposition.  A child
  // window is always below that popup, so it disappears whenever the preview
  // is active.  Keep the debug panel as a second owned popup instead.  Both
  // windows remain tied to the owner lifetime, while HWND_TOP lets the panel
  // stay above the transient preview without making it globally topmost.
  overlay_=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,kDebugUiClass,
      L"Axiom ImGui Debug UI",WS_POPUP|WS_CLIPCHILDREN,0,0,kPanelWidth,
      kPanelHeight,w,nullptr,i,this);
  if(!overlay_)return false;
  context_=nullptr;
  initialized_=true;
  visible_=false;
  ShowWindow(overlay_,SW_HIDE);
  syncOverlay();
  return true;
}
void WindowsDebugUiHost::shutdown()noexcept{if(overlay_)DestroyWindow(overlay_);overlay_=nullptr;context_=nullptr;initialized_=false;visible_=false;placementValid_=false;placementShown_=false;window_=nullptr;}
void WindowsDebugUiHost::toggle()noexcept{visible_=!visible_;placementValid_=false;syncOverlay();if(overlay_)InvalidateRect(overlay_,nullptr,FALSE);}
bool WindowsDebugUiHost::handleMessage(HWND,UINT,WPARAM,LPARAM){return false;}
void WindowsDebugUiHost::syncOverlay()noexcept{
  if(!overlay_||!window_)return;
  RECT c{};
  if(!GetClientRect(window_,&c))return;
  if(IsIconic(window_)){
    ShowWindow(overlay_,SW_HIDE);
    return;
  }
  POINT origin{18,68};
  if(!ClientToScreen(window_,&origin))return;
  const int height=(std::min)(kPanelHeight,(std::max)(static_cast<int>(c.bottom)-70,1));
  const bool shown=visible_;
  if (placementValid_ && placementX_ == origin.x && placementY_ == origin.y &&
      placementWidth_ == kPanelWidth && placementHeight_ == height &&
      placementShown_ == shown) return;
  const UINT flags=SWP_NOACTIVATE|(shown?SWP_SHOWWINDOW:SWP_HIDEWINDOW);
  // Only change z-order when placement actually changes.  Repeated HWND_TOP
  // calls from the 60 Hz diagnostics tick make DWM briefly recompose the
  // popup and were the source of visible panel flicker.
  SetWindowPos(overlay_,shown?HWND_TOP:nullptr,origin.x,origin.y,kPanelWidth,height,
               flags|(shown?0U:SWP_NOZORDER));
  placementX_=origin.x; placementY_=origin.y; placementWidth_=kPanelWidth;
  placementHeight_=height; placementShown_=shown; placementValid_=true;
}
void WindowsDebugUiHost::reposition()noexcept{placementValid_=false;syncOverlay();}
void WindowsDebugUiHost::raise()noexcept{
  if(!overlay_||!window_||!visible_||IsIconic(window_))return;
  SetWindowPos(overlay_,HWND_TOP,0,0,0,0,
               SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void WindowsDebugUiHost::frame(const DebugSnapshot& s){snapshot_=s;if(!initialized_)return;syncOverlay();if(visible_){raise();InvalidateRect(overlay_,nullptr,FALSE);}}
void WindowsDebugUiHost::selectToolAt(int x,int y){if(!visible_||x<14||x>396||y<320||y>470)return;int row=(y-320)/25;constexpr std::array<int,6> ids{4101,4102,4103,4104,4105,4106};if(row>=0&&row<6&&toolSelector_&&toolSelector_(ids[static_cast<size_t>(row)])){selectedTool_=ids[static_cast<size_t>(row)];InvalidateRect(overlay_,nullptr,FALSE);}}
void WindowsDebugUiHost::paintOverlay(HDC dc)const{fill(dc,0,0,kPanelWidth,kPanelHeight,RGB(24,28,36));fill(dc,0,0,kPanelWidth,34,RGB(38,48,66));text(dc,14,9,L"Axiom ImGui Debug UI",RGB(245,185,70));text(dc,14,39,L"P32 controller / reference profile");constexpr std::array<std::pair<const wchar_t*,Capability>,8> panels{{{L"Overview",Capability::kTelemetry},{L"Input",Capability::kInput},{L"Canvas",Capability::kCanonicalSurface},{L"Arc Preview",Capability::kArcPreviewSurface},{L"Surface",Capability::kSurfaceMode},{L"Brush",Capability::kCanonicalSurface},{L"Telemetry",Capability::kTelemetry},{L"Inspection",Capability::kInspection}}};int y=68;for(const auto& p:panels){auto s=snapshot_.capability(p.second);bool ok=s==CapabilityState::kAvailable;fill(dc,14,y-2,kPanelWidth-14,y+20,ok?RGB(42,55,70):RGB(48,48,52));text(dc,24,y,std::wstring(p.first)+L"  "+stateName(s),ok?RGB(225,230,238):RGB(145,150,160));y+=27;}text(dc,14,296,L"Brush / Eraser",RGB(245,185,70));constexpr std::array<std::pair<const wchar_t*,int>,6> tools{{{L"Vector",4101},{L"Marker",4102},{L"Chalk",4103},{L"Membrane",4104},{L"Object Eraser",4105},{L"Partial Eraser",4106}}};y=320;for(const auto& t:tools){bool sel=selectedTool_==t.second;fill(dc,14,y-2,kPanelWidth-14,y+20,sel?RGB(76,94,122):RGB(42,55,70));text(dc,24,y,(sel?L"● ":L"○ ")+std::wstring(t.first));y+=25;}std::wstringstream l;l<<L"gen "<<snapshot_.stamp.generation<<L" seq "<<snapshot_.stamp.sequence<<L" pointers "<<snapshot_.activePointerCount;text(dc,14,482,l.str(),RGB(190,205,220));l.str(L"");l<<L"canonical "<<snapshot_.canonicalRevision<<L" preview "<<snapshot_.previewRevision;text(dc,14,504,l.str(),RGB(190,205,220));}
LRESULT CALLBACK WindowsDebugUiHost::overlayWindowProc(HWND w,UINT m,WPARAM wp,LPARAM lp){auto* s=reinterpret_cast<WindowsDebugUiHost*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(m==WM_NCCREATE){auto* c=reinterpret_cast<CREATESTRUCTW*>(lp);s=static_cast<WindowsDebugUiHost*>(c->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(w,m,wp,lp);if(m==WM_LBUTTONDOWN){s->selectToolAt(static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp)));return 0;}if(m==WM_PAINT){PAINTSTRUCT p{};HDC dc=BeginPaint(w,&p);s->paintOverlay(dc);EndPaint(w,&p);return 0;}if(m==WM_ERASEBKGND)return 1;return DefWindowProcW(w,m,wp,lp);}
void WindowsDebugUiHost::paint(HDC)const{}
} // namespace canvas::debug_ui
#endif
