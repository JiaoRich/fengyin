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

// Test-only input preparation: focus the actual File name Edit and send OS
// keyboard input. Confirm using real mouse/touch Open, never OnFileOk/IPC fakes.
inline HWND prepareNativeVideoDialog(HWND dialog, const juce::File& file, bool cancel = false)
{
    if (cancel) return GetDlgItem(dialog, IDCANCEL);
    if(GetForegroundWindow()!=dialog)return nullptr;
    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root, edit;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> id, type, both;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))
        || FAILED(automation->ElementFromHandle(dialog, &root))) return nullptr;
    VARIANT name; VariantInit(&name); name.vt = VT_BSTR; name.bstrVal = SysAllocString(L"1148");
    const auto conditionResult = automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, name, &id);
    VariantClear(&name);
    VARIANT kind;VariantInit(&kind);kind.vt=VT_I4;kind.lVal=UIA_EditControlTypeId;
    if (FAILED(conditionResult)||FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId,kind,&type))
        ||FAILED(automation->CreateAndCondition(id.Get(),type.Get(),&both))
        ||FAILED(root->FindFirst(TreeScope_Descendants,both.Get(),&edit))||!edit)return nullptr;
    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> value;
    if (FAILED(edit->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value)))) return nullptr;
    BSTR current=nullptr;
    if(FAILED(value->get_CurrentValue(&current)))return nullptr;
    const auto actual=juce::String(current);SysFreeString(current);
    const auto wanted=file.getFullPathName();
    if(actual!=wanted) {
        static HWND typedWindow=nullptr;
        static double typedAt=0;
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if(typedWindow!=dialog||now-typedAt>1000) {
            if(FAILED(edit->SetFocus()))return nullptr;
            std::vector<INPUT> input;
            const auto key=[&](WORD vk,DWORD flags,WORD scan=0){INPUT item{};item.type=INPUT_KEYBOARD;item.ki.wVk=vk;item.ki.wScan=scan;item.ki.dwFlags=flags;input.push_back(item);};
            key(VK_CONTROL,0);key('A',0);key('A',KEYEVENTF_KEYUP);key(VK_CONTROL,KEYEVENTF_KEYUP);
            const std::wstring text(wanted.toWideCharPointer());
            for(const auto character:text){key(0,KEYEVENTF_UNICODE,WORD(character));key(0,KEYEVENTF_UNICODE|KEYEVENTF_KEYUP,WORD(character));}
            if(SendInput(UINT(input.size()),input.data(),sizeof(INPUT))!=input.size())return nullptr;
            typedWindow=dialog;typedAt=now;
        }
        // Wait for the real Edit to receive the OS keyboard events. A parent
        // ComboBox shares ID 1148; its ValuePattern can report success without
        // filling the Edit. Never click Open until the actual text matches.
        return nullptr;
    }
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
