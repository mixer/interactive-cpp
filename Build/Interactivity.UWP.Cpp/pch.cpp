#include "pch.h"

#ifdef ANDROID
template< typename T >
std::string std::to_string(const T& item) {

	std::stringstream ss;
	ss << item;
	return ss.str();

}

#endif