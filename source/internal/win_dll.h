#pragma once

#include "windows.h"
#include <type_traits>

namespace mixer_internal
{

class win_proc_ptr
{
public:
	explicit win_proc_ptr(FARPROC ptr);
	template <typename T, typename = std::enable_if_t<std::is_function_v<T>>>
	operator T *() const;

private:
	FARPROC m_ptr;
};

class win_dll
{
public:
	explicit win_dll(const char* filename);
	~win_dll();

	win_proc_ptr operator[](const char* processName) const;

private:
	HMODULE m_hmod;
};
}
