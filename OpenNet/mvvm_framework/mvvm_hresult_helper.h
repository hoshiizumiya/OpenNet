#pragma once
#ifndef __MVVM_CPPWINRT_MVVM_HRESULT_HELPER_H_INCLUDED__
#define __MVVM_CPPWINRT_MVVM_HRESULT_HELPER_H_INCLUDED__

#include <winerror.h> // for HRESULT_FROM_WIN32, ERROR_CANCELLED
#ifndef ERROR_CANCELLED
#define ERROR_CANCELLED 1223L
#endif
#include <cstdint>

namespace mvvm::HResultHelper {
    inline constexpr int32_t hresult_error_fCanceled() noexcept {
        return static_cast<int32_t>(HRESULT_FROM_WIN32(ERROR_CANCELLED)); // 0x800704C7
    }
}

#endif // __MVVM_CPPWINRT_MVVM_HRESULT_HELPER_H_INCLUDED__
