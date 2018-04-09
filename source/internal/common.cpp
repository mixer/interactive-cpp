#include "common.h"
#include "json.h"
#include "debugging.h"

#include "rapidjson\stringbuffer.h"
#include "rapidjson\writer.h"

namespace mixer
{

// String conversion functions
std::wstring utf8_to_wstring(const char* str)
{
	std::wstring out;
	unsigned int codepoint;
	while (*str != 0)
	{
		unsigned char ch = static_cast<unsigned char>(*str);
		if (ch <= 0x7f)
		{
			codepoint = ch;
		}
		else if (ch <= 0xbf)
		{
			codepoint = (codepoint << 6) | (ch & 0x3f);
		}
		else if (ch <= 0xdf)
		{
			codepoint = ch & 0x1f;
		}
		else if (ch <= 0xef)
		{
			codepoint = ch & 0x0f;
		}
		else
		{
			codepoint = ch & 0x07;
		}

		++str;
		if (((*str & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
		{
			if (codepoint > 0xffff)
			{
				out.append(1, static_cast<wchar_t>(0xd800 + (codepoint >> 10)));
				out.append(1, static_cast<wchar_t>(0xdc00 + (codepoint & 0x03ff)));
			}
			else if (codepoint < 0xd800 || codepoint >= 0xe000)
			{
				out.append(1, static_cast<wchar_t>(codepoint));
			}
		}
	}
	return out;
}

std::wstring utf8_to_wstring(const std::string& str)
{
	return utf8_to_wstring(str.c_str());
}

std::string wstring_to_utf8(const wchar_t* wstr)
{	
	std::string out;
    unsigned int codepoint = 0;
    for (wstr;  *wstr != 0;  ++wstr)
    {
		if (*wstr >= 0xd800 && *wstr <= 0xdbff)
		{
			codepoint = ((*wstr - 0xd800) << 10) + 0x10000;
		}
        else
        {
			if (*wstr >= 0xdc00 && *wstr <= 0xdfff)
			{
				codepoint |= *wstr - 0xdc00;
			}
			else
			{
				codepoint = *wstr;
			}

			if (codepoint <= 0x7f)
			{
				out.append(1, static_cast<char>(codepoint));
			}
            else if (codepoint <= 0x7ff)
            {
                out.append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
                out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
            }
            else if (codepoint <= 0xffff)
            {
                out.append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
                out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
            }
            else
            {
                out.append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
                out.append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
                out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
            }

            codepoint = 0;
        }
    }
    return out;
}

std::string wstring_to_utf8(const std::wstring& wstr)
{
	return wstring_to_utf8(wstr.c_str());
}

// JSON

std::string jsonStringify(rapidjson::Value& value)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	value.Accept(writer);
	return std::string(buffer.GetString(), buffer.GetSize());
}

// Debugging

void interactive_config_debug_level(const interactive_debug_level dbgLevel)
{
	g_dbgLevel = dbgLevel;
}

void interactive_config_debug(const interactive_debug_level dbgLevel, const on_debug_msg dbgCallback)
{
	g_dbgLevel = dbgLevel;
	g_dbgCallback = dbgCallback;
}

}