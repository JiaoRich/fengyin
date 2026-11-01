#pragma once
#if JUCE_WINDOWS
#include <UIAutomation.h>
#include <wrl/client.h>
#include <iostream>

inline void dumpNativeVideoDialog(DWORD pid)
{
    const auto foreground=GetForegroundWindow();DWORD foregroundPid=0;GetWindowThreadProcessId(foreground,&foregroundPid);
    std::cerr<<"foreground="<<foreground<<" pid="<<foregroundPid<<std::endl;
    const auto describe=[](HWND window,LPARAM)->BOOL {
        wchar_t name[128]{},text[512]{};GetClassNameW(window,name,128);DWORD_PTR ignored=0;
        SendMessageTimeoutW(window,WM_GETTEXT,512,reinterpret_cast<LPARAM>(text),SMTO_ABORTIFHUNG,100,&ignored);
        std::cerr<<"hwnd="<<window<<" id="<<GetDlgCtrlID(window)<<" class="<<juce::String(name)
            <<" text="<<juce::String(text)<<" visible="<<IsWindowVisible(window)<<" enabled="<<IsWindowEnabled(window)<<std::endl;return TRUE;
    };
    struct Context {DWORD pid;WNDENUMPROC describe;} context{pid,describe};
    EnumWindows([](HWND window,LPARAM p)->BOOL {
        const auto& context=*reinterpret_cast<Context*>(p);DWORD windowPid=0;GetWindowThreadProcessId(window,&windowPid);
        if(windowPid==context.pid){context.describe(window,0);EnumChildWindows(window,context.describe,0);}return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    const auto screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);
    const int width=GetSystemMetrics(SM_CXSCREEN),height=GetSystemMetrics(SM_CYSCREEN);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;const auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(bitmap&&pixels){
        const auto previous=SelectObject(memory,bitmap);
        if(BitBlt(memory,0,0,width,height,screen,0,0,SRCCOPY|CAPTUREBLT)) {
            BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+width*height*4;
            juce::FileOutputStream output(juce::File::getCurrentWorkingDirectory().getChildFile("native-selector-failure.bmp"));
            output.write(&header,sizeof(header));output.write(&info.bmiHeader,sizeof(info.bmiHeader));output.write(pixels,size_t(width)*size_t(height)*4);
        }
        SelectObject(memory,previous);DeleteObject(bitmap);
    }
    DeleteDC(memory);ReleaseDC(nullptr,screen);
}

// Test-only input preparation. Confirmation still goes through the real OS
// mouse/touch Open button; never invoke OnFileOk or synthesize a result file.
inline HWND prepareNativeVideoDialog(HWND dialog, const juce::File& file, bool cancel = false)
{
    if (cancel) return GetDlgItem(dialog, IDCANCEL);
    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root, edit;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> id;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))
        || FAILED(automation->ElementFromHandle(dialog, &root))) return nullptr;
    VARIANT name; VariantInit(&name); name.vt = VT_BSTR; name.bstrVal = SysAllocString(L"1148");
    const auto conditionResult = automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, name, &id);
    VariantClear(&name);
    if (FAILED(conditionResult) || FAILED(root->FindFirst(TreeScope_Descendants, id.Get(), &edit)) || !edit) return nullptr;
    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> value;
    if (FAILED(edit->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value)))) return nullptr;
    const auto path = SysAllocString(file.getFullPathName().toWideCharPointer());
    if (!path) return nullptr;
    const auto setResult = value->SetValue(path);
    SysFreeString(path);
    if (FAILED(setResult)) return nullptr;
    return GetDlgItem(dialog, IDOK);
}

inline bool nativeDialogButtonPoint(HWND dialog, HWND button, POINT& point)
{
    RECT bounds {};
    if (!button || !IsWindowVisible(dialog) || !IsWindowEnabled(dialog) || !IsWindowEnabled(button)
        || !GetWindowRect(button, &bounds)) return false;
    point = {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
    return GetAncestor(WindowFromPoint(point), GA_ROOT) == dialog && GetForegroundWindow() == dialog;
}
#endif
