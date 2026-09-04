#pragma once

#include <windows.h>
#include <string>

namespace pcdr {

class PhysicalDisk {
public:
    static bool GetDiskNumberForVolume(wchar_t letter, DWORD& diskNumber);
    static bool GetDiskModelSerial(DWORD diskNumber, std::wstring& model, std::wstring& serial);
    static bool IsRemovableOrHotplug(DWORD diskNumber);
    static bool IsUsbBus(DWORD diskNumber);
};

} // namespace pcdr
