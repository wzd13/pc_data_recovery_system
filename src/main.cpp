#include "ui/AppWindow.hpp"
#include "utils/Logger.hpp"
#include "utils/PathUtil.hpp"
#include "settings/Settings.hpp"

#include <windows.h>
#include <objbase.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    pcdr::Settings::Instance().Load();
    pcdr::Logger::Instance().SetLogDirectory(pcdr::pathutil::GetLogsDirectory());

    pcdr::AppWindow app;
    if (!app.Create(instance)) {
        MessageBoxW(nullptr, L"Failed to create application window.", L"PC Data Recovery", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const int code = app.Run();
    CoUninitialize();
    return code;
}
