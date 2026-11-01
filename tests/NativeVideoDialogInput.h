#pragma once
#if JUCE_WINDOWS
#include <UIAutomation.h>
#include <wrl/client.h>

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
