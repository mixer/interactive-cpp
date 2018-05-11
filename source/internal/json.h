#pragma once

#ifndef RAPIDJSON_HAS_STDSTRING 
#define RAPIDJSON_HAS_STDSTRING 1
#elif RAPIDJSON_HAS_STDSTRING == 0
#error "Mixer interactivity requires std::string for json parsing."
#endif
#include "rapidjson\document.h"

namespace mixer_internal
{

std::string jsonStringify(rapidjson::Value& doc);

}