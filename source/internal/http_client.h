#pragma once

#include <string>
#include <memory>
#include <map>

namespace mixer_internal
{

typedef std::map<std::string, std::string> http_headers;

struct http_response
{
	unsigned int statusCode;
	std::string body;
};

class http_client
{
public:
	virtual ~http_client() = 0 {};

	// Make an http request with optional headers
	virtual int make_request(const std::string& uri, const std::string& requestType, const http_headers* headers, const std::string& body, _Out_ http_response& response, unsigned long timeoutMs = 5000) const = 0;
};

class http_factory
{
public:
	static std::unique_ptr<http_client> make_http_client();
};

}