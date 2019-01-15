#include "win_dll.h"

namespace mixer_internal
{

win_proc_ptr::win_proc_ptr(FARPROC ptr) : m_ptr(ptr) {};

template <typename T, typename>
win_proc_ptr::operator T *() const
{
	return reinterpret_cast<T *>(m_ptr);
};

win_dll::win_dll(const char* filename) : m_hmod(LoadLibraryA(filename)) {};
win_dll::~win_dll()
{
	FreeLibrary(m_hmod);
};

win_proc_ptr win_dll::operator[](const char* processName) const
{
	return win_proc_ptr(GetProcAddress(m_hmod, processName));
};
}