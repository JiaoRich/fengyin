#include <windows.h>
#include <audioclient.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

using Microsoft::WRL::ComPtr;

namespace
{
constexpr wchar_t windowClassName[] = L"FengYinWasapiPeriodProbeWindow";
constexpr int resultControlId = 1001;
constexpr int refreshButtonId = 1002;
constexpr int copyButtonId = 1003;

HWND resultEdit = nullptr;

std::wstring hrText(HRESULT hr)
{
    wchar_t* message = nullptr;
    const auto size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                         | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr,
                                     static_cast<DWORD>(hr),
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     reinterpret_cast<wchar_t*>(&message),
                                     0,
                                     nullptr);

    std::wostringstream out;
    out << L"0x" << std::uppercase << std::hex << std::setw(8) << std::setfill(L'0')
        << static_cast<unsigned long>(hr);
    if (size != 0 && message != nullptr)
    {
        std::wstring text(message, size);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
            text.pop_back();
        out << L"（" << text << L"）";
    }
    if (message != nullptr)
        LocalFree(message);
    return out.str();
}

std::wstring formatName(const WAVEFORMATEX* format)
{
    if (format == nullptr)
        return L"未知";

    std::wostringstream out;
    out << format->nSamplesPerSec << L" Hz / " << format->wBitsPerSample << L" bit / "
        << format->nChannels << L" 声道";

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        out << L" / Float";
    else if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            out << L" / Float";
        else if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
            out << L" / PCM";
    }
    else if (format->wFormatTag == WAVE_FORMAT_PCM)
        out << L" / PCM";
    return out.str();
}

std::wstring periodText(UINT32 frames, UINT32 sampleRate)
{
    std::wostringstream out;
    const double milliseconds = sampleRate == 0 ? 0.0 : 1000.0 * frames / sampleRate;
    out << frames << L" samples（" << std::fixed << std::setprecision(2) << milliseconds << L" ms）";
    return out.str();
}

std::wstring getFriendlyName(IMMDevice* device)
{
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties)))
        return L"未命名设备";

    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring name = L"未命名设备";
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
        && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
        name = value.pwszVal;
    PropVariantClear(&value);
    return name;
}

void inspectMode(IMMDevice* device, bool raw, std::wostringstream& out)
{
    ComPtr<IAudioClient3> client;
    auto hr = device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(client.GetAddressOf()));
    out << L"\r\n  处理模式：" << (raw ? L"RAW" : L"Default") << L"\r\n";
    if (FAILED(hr))
    {
        out << L"  IAudioClient3：不可用，" << hrText(hr) << L"\r\n";
        return;
    }

    AudioClientProperties properties{};
    properties.cbSize = sizeof(properties);
    properties.bIsOffload = FALSE;
    properties.eCategory = AudioCategory_ProAudio;
    properties.Options = raw ? AUDCLNT_STREAMOPTIONS_RAW : AUDCLNT_STREAMOPTIONS_NONE;
    hr = client->SetClientProperties(&properties);
    if (FAILED(hr))
    {
        out << L"  设置模式：失败，" << hrText(hr) << L"\r\n";
        return;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = client->GetMixFormat(&mixFormat);
    if (FAILED(hr) || mixFormat == nullptr)
    {
        out << L"  获取混音格式：失败，" << hrText(hr) << L"\r\n";
        return;
    }

    out << L"  混音格式：" << formatName(mixFormat) << L"\r\n";

    WAVEFORMATEX* currentFormat = nullptr;
    UINT32 currentPeriod = 0;
    hr = client->GetCurrentSharedModeEnginePeriod(&currentFormat, &currentPeriod);
    if (SUCCEEDED(hr) && currentFormat != nullptr)
    {
        out << L"  当前周期：" << periodText(currentPeriod, currentFormat->nSamplesPerSec) << L"\r\n";
        CoTaskMemFree(currentFormat);
    }
    else
    {
        out << L"  当前周期：读取失败，" << hrText(hr) << L"\r\n";
    }

    UINT32 defaultPeriod = 0;
    UINT32 fundamentalPeriod = 0;
    UINT32 minimumPeriod = 0;
    UINT32 maximumPeriod = 0;
    hr = client->GetSharedModeEnginePeriod(mixFormat,
                                            &defaultPeriod,
                                            &fundamentalPeriod,
                                            &minimumPeriod,
                                            &maximumPeriod);
    if (FAILED(hr))
    {
        out << L"  支持周期：读取失败，" << hrText(hr) << L"\r\n";
        CoTaskMemFree(mixFormat);
        return;
    }

    out << L"  默认周期：" << periodText(defaultPeriod, mixFormat->nSamplesPerSec) << L"\r\n";
    out << L"  最小周期：" << periodText(minimumPeriod, mixFormat->nSamplesPerSec) << L"\r\n";
    out << L"  基础步长：" << periodText(fundamentalPeriod, mixFormat->nSamplesPerSec) << L"\r\n";
    out << L"  最大周期：" << periodText(maximumPeriod, mixFormat->nSamplesPerSec) << L"\r\n";

    hr = client->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                              minimumPeriod,
                                              mixFormat,
                                              nullptr);
    if (SUCCEEDED(hr))
    {
        out << L"  最低周期申请：成功\r\n";
    }
    else if (hr == AUDCLNT_E_ENGINE_PERIODICITY_LOCKED)
    {
        out << L"  最低周期申请：被其他音频流锁定；关闭正在发声的软件后重测\r\n";
    }
    else if (hr == AUDCLNT_E_RAW_MODE_UNSUPPORTED)
    {
        out << L"  最低周期申请：该端点不支持 RAW\r\n";
    }
    else
    {
        out << L"  最低周期申请：失败，" << hrText(hr) << L"\r\n";
    }

    CoTaskMemFree(mixFormat);
}

std::wstring runProbe()
{
    std::wostringstream out;
    out << L"风吟 WASAPI 共享周期检测工具 1.0\r\n"
        << L"检测时间：";

    SYSTEMTIME now{};
    GetLocalTime(&now);
    out << now.wYear << L"-" << std::setfill(L'0') << std::setw(2) << now.wMonth << L"-"
        << std::setw(2) << now.wDay << L" " << std::setw(2) << now.wHour << L":"
        << std::setw(2) << now.wMinute << L":" << std::setw(2) << now.wSecond << L"\r\n"
        << L"说明：以下数值由 Windows IAudioClient3 直接读取，不是风吟下拉框估算。\r\n";

    ComPtr<IMMDeviceEnumerator> enumerator;
    auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(enumerator.GetAddressOf()));
    if (FAILED(hr))
    {
        out << L"\r\n无法创建音频设备枚举器：" << hrText(hr) << L"\r\n";
        return out.str();
    }

    ComPtr<IMMDeviceCollection> devices;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    if (FAILED(hr))
    {
        out << L"\r\n无法枚举输出设备：" << hrText(hr) << L"\r\n";
        return out.str();
    }

    UINT count = 0;
    devices->GetCount(&count);
    if (count == 0)
    {
        out << L"\r\n没有发现可用的音频输出设备。\r\n";
        return out.str();
    }

    ComPtr<IMMDevice> defaultDevice;
    LPWSTR defaultId = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice))
        && SUCCEEDED(defaultDevice->GetId(&defaultId)))
    {
        // defaultId is compared below and released after enumeration.
    }

    for (UINT index = 0; index < count; ++index)
    {
        ComPtr<IMMDevice> device;
        if (FAILED(devices->Item(index, &device)))
            continue;

        LPWSTR id = nullptr;
        device->GetId(&id);
        const bool isDefault = id != nullptr && defaultId != nullptr && wcscmp(id, defaultId) == 0;

        out << L"\r\n============================================================\r\n"
            << L"设备 " << (index + 1) << L"：" << getFriendlyName(device.Get())
            << (isDefault ? L"  [系统默认]" : L"") << L"\r\n";
        if (id != nullptr)
            out << L"端点 ID：" << id << L"\r\n";

        inspectMode(device.Get(), false, out);
        inspectMode(device.Get(), true, out);

        if (id != nullptr)
            CoTaskMemFree(id);
    }

    if (defaultId != nullptr)
        CoTaskMemFree(defaultId);

    out << L"\r\n============================================================\r\n"
        << L"快速判断：48 kHz 下，128=2.67ms，256=5.33ms，480=10ms，512=10.67ms。\r\n"
        << L"若最小周期较小但申请被锁定，请关闭浏览器、播放器和其他音频软件后重新检测。\r\n";
    return out.str();
}

void setResults()
{
    SetWindowTextW(resultEdit, L"正在检测，请稍候……");
    const auto result = runProbe();
    SetWindowTextW(resultEdit, result.c_str());
}

void copyResults(HWND owner)
{
    const int length = GetWindowTextLengthW(resultEdit);
    if (length <= 0 || !OpenClipboard(owner))
        return;

    EmptyClipboard();
    const SIZE_T bytes = static_cast<SIZE_T>(length + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr)
    {
        auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
        if (destination != nullptr)
        {
            GetWindowTextW(resultEdit, destination, length + 1);
            GlobalUnlock(memory);
            if (SetClipboardData(CF_UNICODETEXT, memory) != nullptr)
                memory = nullptr;
        }
        if (memory != nullptr)
            GlobalFree(memory);
    }
    CloseClipboard();
}

void layout(HWND window)
{
    RECT area{};
    GetClientRect(window, &area);
    constexpr int margin = 14;
    constexpr int buttonHeight = 36;
    constexpr int gap = 10;
    const int buttonWidth = 130;
    const int bottom = area.bottom - margin - buttonHeight;

    MoveWindow(resultEdit,
               margin,
               margin,
               std::max(100, static_cast<int>(area.right) - margin * 2),
               std::max(100, bottom - margin - gap),
               TRUE);
    MoveWindow(GetDlgItem(window, refreshButtonId),
               margin,
               bottom,
               buttonWidth,
               buttonHeight,
               TRUE);
    MoveWindow(GetDlgItem(window, copyButtonId),
               margin + buttonWidth + gap,
               bottom,
               buttonWidth,
               buttonHeight,
               TRUE);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            resultEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
                                         L"EDIT",
                                         L"",
                                         WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT
                                             | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
                                         0, 0, 0, 0,
                                         window,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(resultControlId)),
                                         nullptr,
                                         nullptr);
            CreateWindowW(L"BUTTON", L"重新检测", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(refreshButtonId)), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"复制结果", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(copyButtonId)), nullptr, nullptr);

            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(resultEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(GetDlgItem(window, refreshButtonId), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            SendMessageW(GetDlgItem(window, copyButtonId), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            PostMessageW(window, WM_APP + 1, 0, 0);
            return 0;
        }
        case WM_SIZE:
            layout(window);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == refreshButtonId)
                setResults();
            else if (LOWORD(wParam) == copyButtonId)
                copyResults(window);
            return 0;
        case WM_APP + 1:
            setResults();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    const auto comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        MessageBoxW(nullptr, L"无法初始化 Windows 音频检测环境。", L"风吟 WASAPI 周期检测", MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = windowClassName;
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(0,
                                  windowClassName,
                                  L"风吟 WASAPI 共享周期检测工具",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  980,
                                  720,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (window == nullptr)
    {
        CoUninitialize();
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();
    return static_cast<int>(message.wParam);
}
