#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>

namespace broapps::win_backend {

class ComScope {
public:
    explicit ComScope(DWORD coinit = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) {
        hr_ = CoInitializeEx(nullptr, coinit);
    }

    ~ComScope() {
        if (SUCCEEDED(hr_)) {
            CoUninitialize();
        }
    }

    bool succeeded() const { return SUCCEEDED(hr_); }
    HRESULT result() const { return hr_; }

    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

private:
    HRESULT hr_ = E_FAIL;
};

}  // namespace broapps::win_backend
#endif
