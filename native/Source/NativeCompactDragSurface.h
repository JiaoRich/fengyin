#pragma once
#if JUCE_WINDOWS
#include <windows.h>
#include <windowsx.h>

// A tiny, virtually transparent child over the non-interactive title area.
// Input never crosses WebView IPC while the HWND underneath is moving.
class NativeCompactDragSurface {
    HWND surface=nullptr,host=nullptr;
    RECT initial{};
    POINT start{};
    UINT32 pointer=0;
    bool mouse=false;
    RECT region{};
    DWORD creationError=ERROR_SUCCESS;
    void begin(POINT point) {
        GetWindowRect(host,&initial);start=point;
        SetForegroundWindow(host);
    }
    void move(POINT point) {
        SetWindowPos(host,nullptr,initial.left+point.x-start.x,initial.top+point.y-start.y,
                     0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    void cancel() {
        pointer=0;mouse=false;
        if(GetCapture()==surface)ReleaseCapture();
    }
    static LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<NativeCompactDragSurface*>(GetWindowLongPtr(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<NativeCompactDragSurface*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtr(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(!self)return DefWindowProcW(hwnd,message,w,l);
        switch(message) {
            case WM_ERASEBKGND:return 1;
            case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);FillRect(dc,&ps.rcPaint,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));EndPaint(hwnd,&ps);return 0;}
            case WM_LBUTTONDOWN:{if(self->pointer)return 0;POINT p{};GetCursorPos(&p);self->mouse=true;self->begin(p);SetCapture(hwnd);return 0;}
            case WM_MOUSEMOVE:if(self->mouse){if(!(w&MK_LBUTTON)){self->cancel();return 0;}POINT p{};GetCursorPos(&p);self->move(p);}return 0;
            case WM_LBUTTONUP:self->cancel();return 0;
            case WM_POINTERDOWN:{
                POINTER_INFO info{};const auto id=GET_POINTERID_WPARAM(w);
                if(!self->pointer&&!self->mouse&&GetPointerInfo(id,&info)&&(info.pointerFlags&POINTER_FLAG_PRIMARY)) {
                    self->pointer=id;self->begin(info.ptPixelLocation);
                }
                return 0;
            }
            case WM_POINTERUPDATE:{
                if(GET_POINTERID_WPARAM(w)==self->pointer) {
                    POINTER_INFO info{};
                    if(!GetPointerInfo(self->pointer,&info)||(info.pointerFlags&POINTER_FLAG_CANCELED)||!(info.pointerFlags&POINTER_FLAG_INCONTACT))self->cancel();
                    else self->move(info.ptPixelLocation);
                }
                return 0;
            }
            case WM_POINTERUP:if(GET_POINTERID_WPARAM(w)==self->pointer)self->cancel();return 0;
            case WM_POINTERCAPTURECHANGED:if(GET_POINTERID_WPARAM(w)==self->pointer)self->cancel();return 0;
            case WM_CAPTURECHANGED:self->mouse=false;return 0;
            case WM_CANCELMODE:self->cancel();return 0;
            default:return DefWindowProcW(hwnd,message,w,l);
        }
    }
public:
    DWORD lastCreationError() const {return creationError;}
    ~NativeCompactDragSurface(){if(surface)DestroyWindow(surface);}
    void update(HWND parent,int x,int y,int width,int height,bool enabled) {
        host=parent;
        if(!enabled||width<=0||height<=0){if(surface){cancel();ShowWindow(surface,SW_HIDE);}return;}
        if(!surface) {
            static const ATOM registered=[] {
                WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandle(nullptr);
                wc.lpszClassName=L"FengYinCompactDragSurface";wc.hCursor=LoadCursor(nullptr,IDC_SIZEALL);
                return RegisterClassW(&wc);
            }();
            if(!registered){creationError=GetLastError();return;}
            surface=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE,L"FengYinCompactDragSurface",L"",
                                    WS_CHILD,x,y,width,height,host,nullptr,GetModuleHandle(nullptr),this);
            if(!surface){creationError=GetLastError();return;}
            if(!SetLayeredWindowAttributes(surface,0,1,LWA_ALPHA)) {
                creationError=GetLastError();DestroyWindow(surface);surface=nullptr;return;
            }
            creationError=ERROR_SUCCESS;
        }
        const RECT next{x,y,x+width,y+height};
        if(!EqualRect(&region,&next)||!IsWindowVisible(surface)) {
            region=next;
            SetWindowPos(surface,HWND_TOP,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }
    }
};
#endif
