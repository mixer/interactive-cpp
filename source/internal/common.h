#pragma once

#include <string>

namespace mixer
{

#define RETURN_IF_FAILED(x) do { int __err_c = x; if(0 != __err_c) return __err_c; } while(0)

// String conversion functions
std::wstring utf8_to_wstring(const char* str);
std::wstring utf8_to_wstring(const std::string& str);
std::string wstring_to_utf8(const wchar_t* wstr);
std::string wstring_to_utf8(const std::wstring& wstr);

}