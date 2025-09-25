#pragma once
#ifndef __MVVM_CPPWINRT_OBJECT_CONVERTER_HPP_INCLUDED
#define __MVVM_CPPWINRT_OBJECT_CONVERTER_HPP_INCLUDED

#include <unknwn.h>
#include <winrt/base.h>
#include <type_traits>
#include <stdexcept>
#include <hstring.h>

namespace COMRefTracking {

    template<class>
    inline constexpr bool always_false = false;

    // 这里判断是不是 WinRT 对象使用 cppwinrt 中静态类型检查的一个技巧
    // Part1: 接口
    // 所有 WinRT 接口都隐式需要 IInspectable；然后，IInspectable 又需要 IUnknown。
    // IUnknown 根据传统的 COM 使用情况定义三种方法：QueryInterface、AddRef 和 Release。
    // 除了 IUnknown 方法之外，IInspectable 还定义了三种方法：
    // GetIids、GetRuntimeClassName 和 GetTrustLevel。
    // 这三种方法允许对象的客户端检索有关该对象的信息。
    // 具体而言，IInspectable.GetRuntimeClassName 使对象的客户端
    // 能够检索可在元数据中解析的 WinRT 类型名，以启用语言投影。
    // Part2: 委托
    // 委托是充当类型安全函数指针的 WinRT 类型。委托本质上是一个简单的 WinRT 对象，
    // 该对象公开从 IUnknown 继承的单个接口，并定义名为 Invoke 的单个方法。
    // 调用委托反过来会调用它引用的方法。 委托通常 (，但不专门) 用于定义 WinRT 事件。
    // WinRT 委托是一种命名类型，并定义方法签名。委托方法签名遵循与接口方法相同的参数规则。
    // Invoke 方法的签名和参数名称必须与委托的定义匹配。
    // 请注意，与 WinRT 接口不同，委托实现 IUnknown，但不实现 IInspectable。
    // 这意味着它们无法在运行时检查类型信息。
    template <typename T>
    concept WinrtInspectableObject = winrt::impl::is_interface<T>::value;

    // 委托类型是很难判断的，他和一般的 COM+ 方法很像，
    // 这里定义了一个 DelegateLikeObject 将会静态断言是否是 COM 对象类型，且不是经典 COM 对象类型。
    // 这只是一个猜测，并不保证一定正确。
    template <typename T>
    concept DelegateLikeObject = winrt::impl::is_com_interface<T>::value &&
        !winrt::impl::is_classic_com_interface<T>::value;
    
    template <typename T>
    concept WinrtCompatibleObject = std::disjunction<std::is_base_of<winrt::Windows::Foundation::IInspectable, T>, winrt::impl::is_com_interface<T>>::value;

    class ObjectConversionException : public std::runtime_error {
    public:
        explicit ObjectConversionException(const char* message)
            : std::runtime_error(message) {}
    };

    struct WINRT_IMPL_NOVTABLE IInspectable_abi_proxy : winrt::Windows::Foundation::IUnknown
    {
        virtual int32_t __stdcall GetIids(
            /* [out] */ uint32_t* count,
            /* [size_is][size_is][out] */ winrt::guid** ids) noexcept = 0;
        virtual int32_t __stdcall GetRuntimeClassName(
            /* [out] */ void** name) noexcept = 0;  // HSTRING*
        virtual int32_t __stdcall GetTrustLevel(
            /* [out] */ winrt::Windows::Foundation::TrustLevel* level) noexcept = 0;
    };

    class ObjectConverter {
    public:

        // =====================================================
        // WinRT 对象转换
        // =====================================================

        // WinRT 对象 -> COM Object Pointer
        template <typename T>
        static winrt::com_ptr<::IUnknown> ToCoObject(T const& obj) {
            if constexpr (WinrtInspectableObject<T>) {
                return obj.as<::IUnknown>();
            }
            else if constexpr (std::is_base_of_v<::IUnknown, T>) {
                ::IUnknown* pUnk = nullptr;
                if (FAILED(const_cast<T&>(obj).QueryInterface( 
                                IID_PPV_ARGS(&pUnk) 
                            ) 
                        )
                    ) {
                    throw ObjectConversionException("QI for IUnknown failed");
                }
                winrt::com_ptr<::IUnknown> result;
                result.attach(pUnk); // 接管引用计数
                return result;
            }
            else {
                static_assert(always_false<T>, "Unsupported type for ToCoObject conversion");
            }
        }

        // WinRT 对象 -> WinRT IUnknown (强引用)
        template <typename T>
        static winrt::Windows::Foundation::IUnknown ToWinrtObject(T const& obj) {
            if constexpr (WinrtInspectableObject<T>) {
                return obj.as<winrt::Windows::Foundation::IUnknown>();
            }
            else if constexpr (std::is_base_of_v<::IUnknown, T>) {
                // 验证是否为真正的WinRT对象
                winrt::com_ptr<winrt::Windows::Foundation::IInspectable> inspectable;
                HRESULT hr = const_cast<T&>(obj).QueryInterface(
                    winrt::guid_of<winrt::Windows::Foundation::IInspectable>(),
                    reinterpret_cast<void**>(inspectable.put()));

                if (FAILED(hr)) {
                    throw ObjectConversionException("Object does not implement IInspectable");
                }

                winrt::Windows::Foundation::IUnknown winrtObj;
                winrt::copy_from_abi(winrtObj, inspectable.get());
                return winrtObj;
            }
            else {
                static_assert(always_false<T>, "Unsupported type for ToWinrtObject conversion");
            }
        }

        // WinRT 对象 -> COM Raw Pointer (不增加引用计数)
        template <typename T>
        static ::IUnknown* ToCoUnknownPointer(T const& obj) {
            if constexpr (WinrtInspectableObject<T>) {
                return winrt::get_unknown(obj);
            }
            else if constexpr (std::is_base_of_v<::IUnknown, T>) {
                return const_cast<T*>(&obj);
            }
            else {
                static_assert(always_false<T>, "Unsupported type for ToRaw conversion");
            }
        }

        template <typename T>
        static ::IUnknown* ToCoUnknownPointer(T const& obj, bool& isWinrtObject) {
            if constexpr (WinrtInspectableObject<T>) {
                isWinrtObject = true;
                return winrt::get_unknown(obj);
            }
            else if constexpr (WinrtCompatibleObject<T>) {
                isWinrtObject = false;
                return winrt::get_unknown(obj);
            }
            else if constexpr (std::is_base_of_v<::IUnknown, T>) {
                isWinrtObject = false;
                return const_cast<T*>(&obj);
            }
            else {
                static_assert(always_false<T>, "Unsupported type for ToRaw conversion");
            }
        }

        // =====================================================
        // COM 对象转换
        // =====================================================

        // COM Object Pointer -> WinRT 对象 (强引用)
        template <typename WinrtInterface>
        static winrt::com_ptr<WinrtInterface> FromCoUnknownObject(winrt::com_ptr<::IUnknown> comObj) {
            if (!comObj) throw ObjectConversionException("Null COM object");

            winrt::com_ptr<WinrtInterface> result;
            HRESULT hr = comObj->QueryInterface(winrt::guid_of<WinrtInterface>(), result.put_void());
            if (FAILED(hr) || !result) {
                throw ObjectConversionException("Failed to convert COM object to WinRT interface");
            }
            return result;
        }

        // COM Object Pointer -> WinRT 对象 (安全版，失败返回 nullptr)
        template <typename WinrtInterface>
        static winrt::com_ptr<WinrtInterface> SafeFromCoUnknownObject(winrt::com_ptr<::IUnknown> comObj) noexcept {
            winrt::com_ptr<WinrtInterface> result;
            if (comObj) {
                comObj->QueryInterface(winrt::guid_of<WinrtInterface>(), result.put_void());
            }
            return result;
        }

        // =====================================================
        // 原始指针转换
        // =====================================================

        // COM Raw Pointer -> WinRT 对象
        template <typename WinrtInterface>
        static winrt::com_ptr<WinrtInterface> FromCoUnknownPointer(::IUnknown* rawPointer) {
            winrt::com_ptr<::IUnknown> comObj;
            comObj.copy_from(rawPointer);
            return FromCoUnknownObject<WinrtInterface>(comObj);
        }

        template <typename WinrtInterface>
        static winrt::com_ptr<WinrtInterface> SafeFromCoUnknownPointer(::IUnknown* rawPointer) {
            winrt::com_ptr<::IUnknown> comObj;
            comObj.copy_from(rawPointer);
            return SafeFromCoUnknownObject<WinrtInterface>(comObj);
        }

        // COM Raw Pointer -> COM Object Pointer (增加引用计数)
        static winrt::com_ptr<::IUnknown> FromPtrToCoObject(::IUnknown* rawPointer) {
            winrt::com_ptr<::IUnknown> result;
            if (rawPointer) {
                result.copy_from(rawPointer);
            }
            return result;
        }

        template <typename T>
        static void** GetVTable(T const& obj, bool& isWinrtObject) {
            ::IUnknown* raw = ToCoUnknownPointer(obj, isWinrtObject);
            if (!raw) {
                throw ObjectConversionException("Null object when accessing vtable");
            }
            return *reinterpret_cast<void***>(raw);
        }

        template <typename T>
        static void** GetVTable(T const& obj) {
            ::IUnknown* raw = ToCoUnknownPointer(obj);
            if (!raw) {
                throw ObjectConversionException("Null object when accessing vtable");
            }
            return *reinterpret_cast<void***>(raw);
        }

        template <size_t Index, typename T>
        static void* GetMethodAddressSafe(T const& obj) {
            bool isWinrtObject = false;
            void** vtable = GetVTable(obj, isWinrtObject);
            if (!vtable || (!isWinrtObject && Index > ::VTableIndex::ComInterfaceMaxIndex) || !vtable[Index]) {
                throw ObjectConversionException("Invalid vtable or method index");
            }
            return vtable[Index];
        }

        template <size_t Index, typename T>
        static void* GetMethodAddress(T const& obj) {
            bool isWinrtObject = false;
            void** vtable = GetVTable(obj, isWinrtObject);
            size_t vtableSize = GuessVTableMethodCount(obj, isWinrtObject);
            if (!vtable || Index >= vtableSize || !vtable[Index]) {
                throw ObjectConversionException("Invalid vtable or method index");
            }
            return vtable[Index];
        }

        template <typename T>
        static size_t GuessVTableMethodCount(T const& obj, size_t maxProbe = 64)
        {
            bool isWinrtObject = false;
            void** vtable = GetVTable(obj, isWinrtObject);
            if (!vtable) {
                throw ObjectConversionException("Null vtable");
            }

            size_t count = 0;
            for (size_t i = 0; i < maxProbe; ++i)
            {
                if (vtable[i] == nullptr) break;

                // 一些虚表可能有连续函数指针，没有空洞
                // 但为了安全可检测是否指针落在可执行内存区
                MEMORY_BASIC_INFORMATION mbi{};
                if (VirtualQuery(vtable[i], &mbi, sizeof(mbi)) == 0 ||
                    !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY)))
                {
                    break;
                }

                ++count;
            }
            return count;
        }

        template <typename T>
        static std::vector<void*> CollectVTableMethods(T const& obj, size_t maxProbe = 64)
        {
            bool isWinrtObject = false;
            size_t count = GuessVTableMethodCount(obj, maxProbe);
            void** vtable = GetVTable(obj, isWinrtObject);

            std::vector<void*> methods;
            methods.reserve(count);

            for (size_t i = 0; i < count; ++i)
            {
                methods.push_back(vtable[i]);
            }
            return methods;
        }

        // 获取 WinRT ABI 虚表
        static winrt::impl::inspectable_abi* GetInspectableAbi(winrt::Windows::Foundation::IInspectable const& obj) {
            return reinterpret_cast<winrt::impl::inspectable_abi*>(winrt::get_abi(obj));
        }

        static winrt::impl::unknown_abi* GetUnknownAbi(winrt::Windows::Foundation::IUnknown const& obj) {
            return reinterpret_cast<winrt::impl::unknown_abi*>(winrt::get_abi(obj));
        }

        static std::vector<winrt::guid> GetIids(winrt::Windows::Foundation::IInspectable const& obj) {
            uint32_t count = 0;
            winrt::guid* ids = nullptr;
            auto abi = GetInspectableAbi(obj);
            if (FAILED(abi->GetIids(&count, &ids))) {
                throw ObjectConversionException("GetIids failed");
            }
            std::vector<winrt::guid> result(ids, ids + count);
            ::CoTaskMemFree(ids);
            return result;
        }

        static winrt::hstring GetRuntimeClassName(winrt::Windows::Foundation::IInspectable const& obj) {
            void* name = nullptr;
            auto abi = GetInspectableAbi(obj);
            if (FAILED(abi->GetRuntimeClassName(&name))) {
                throw ObjectConversionException("GetRuntimeClassName failed");
            }
            return winrt::hstring{ reinterpret_cast<HSTRING>(name), winrt::take_ownership_from_abi };
        }

        static winrt::Windows::Foundation::TrustLevel GetTrustLevel(winrt::Windows::Foundation::IInspectable const& obj) {
            winrt::Windows::Foundation::TrustLevel level;
            auto abi = GetInspectableAbi(obj);
            if (FAILED(abi->GetTrustLevel(&level))) {
                throw ObjectConversionException("GetTrustLevel failed");
            }
            return level;
        }

        template <typename Interface, typename T>
        static bool SupportsInterface(T const& obj) {
            if constexpr (WinrtInspectableObject<T>) {
                return obj.try_as<Interface>() != nullptr;
            }
            else {
                auto comObj = ToCoObject(obj);
                winrt::com_ptr<Interface> result;
                return comObj && SUCCEEDED(  comObj->QueryInterface( winrt::guid_of<Interface>(), result.put_void() )  ) && result;
            }
        }

        template <typename T>
        static uintptr_t GetObjectId(T const& obj) {
            return reinterpret_cast<uintptr_t>(ToCoUnknownPointer(obj));
        }
    };

    // IUnknown 方法的索引常量
    namespace VTableIndex {
        // COM IUnknown 接口定义的方法
        constexpr size_t QueryInterface = 0;
        constexpr size_t AddRef = 1;
        constexpr size_t Release = 2;
        constexpr size_t ComInterfaceMaxIndex = 2;
        constexpr size_t DelegateInterfaceSafeIndex = 2;
        // WinRT IInspectable 接口扩展的方法
        constexpr size_t GetIids = 3;
        constexpr size_t GetRuntimeClassName = 4;
        constexpr size_t GetTrustLevel = 5;
        // 后续则是接口定义的方法，索引从 6 开始
    }
}

#endif // __MVVM_CPPWINRT_OBJECT_CONVERTER_HPP_INCLUDED
