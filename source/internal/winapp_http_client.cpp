#include "winapp_http_client.h"
#include "common.h"
#include <mutex>
#include <condition_variable>
#include <sstream>

#if _DURANGO
#define CALLING_CONVENTION
#include <ixmlhttprequest2.h>
#else
#define CALLING_CONVENTION __stdcall
#include <MsXml6.h>
#endif

#include <wrl.h>
#include <shcore.h>

#define RETURN_HR_IF_FAILED(x) hr = x; if(0 != hr) { return hr; }

namespace mixer
{

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;
using namespace Microsoft::WRL;
using namespace Windows::Storage::Streams;

// Buffer with the required ISequentialStream interface to send data with and IXHR2 request. The only method
// required for use is Read.  IXHR2 will not write to this buffer nor will it use anything from the IDispatch
// interface.
class HttpRequestStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ISequentialStream, IDispatch>
{
public:
	HttpRequestStream() : m_buffer(nullptr), m_seekLocation(0), m_bufferSize(0)
	{
	}

	~HttpRequestStream()
	{
		FreeInternalBuffer();
	}

	// ISequentialStream interface
	HRESULT CALLING_CONVENTION Read(void *buffer, unsigned long bufferSize, unsigned long *bytesRead)
	{
		if (buffer == nullptr)
		{
			return E_INVALIDARG;
		}

		long result = S_OK;

		if (bufferSize + m_seekLocation > m_bufferSize)
		{
			result = S_FALSE;

			// Calculate how many bytes are remaining 
			bufferSize = static_cast<unsigned long>(m_bufferSize - m_seekLocation);
		}

		memcpy(buffer, m_buffer + m_seekLocation, bufferSize);

		*bytesRead = bufferSize;
		m_seekLocation += bufferSize;

		return result;
	}
	HRESULT CALLING_CONVENTION Write(const void *, unsigned long, unsigned long *) { return E_NOTIMPL; }

	// IDispatch interface is required but not used in this context.  The methods are empty.
	HRESULT CALLING_CONVENTION GetTypeInfoCount(unsigned int FAR*) { return E_NOTIMPL; }
	HRESULT CALLING_CONVENTION GetTypeInfo(unsigned int, LCID, ITypeInfo FAR* FAR*) { return E_NOTIMPL; }
	HRESULT CALLING_CONVENTION GetIDsOfNames(REFIID, OLECHAR FAR* FAR*, unsigned int, LCID, DISPID FAR*) { return DISP_E_UNKNOWNNAME; }
	HRESULT CALLING_CONVENTION Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS FAR*, VARIANT FAR*, EXCEPINFO FAR*, unsigned int FAR*) { return S_OK; }

	// Methods created for simplicity when creating and passing along the buffer
	HRESULT CALLING_CONVENTION Open(const void *buffer, size_t bufferSize)
	{
		AllocateInternalBuffer(bufferSize);

		memcpy(m_buffer, buffer, bufferSize);

		return S_OK;
	}
	size_t CALLING_CONVENTION Size() const { return m_bufferSize; }
private:
	void CALLING_CONVENTION AllocateInternalBuffer(size_t size)
	{
		if (m_buffer != nullptr)
		{
			FreeInternalBuffer();
		}

		m_bufferSize = size;
		m_buffer = new unsigned char[size];
	}
	void CALLING_CONVENTION FreeInternalBuffer()
	{
		delete[] m_buffer;
		m_buffer = nullptr;
	}

	unsigned char    *m_buffer;
	size_t            m_seekLocation;
	size_t            m_bufferSize;
};

class RequestCallback : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback>
{
private:
	std::mutex m_responseMutex;
	std::condition_variable m_responseCV;
	bool m_responseReceived;
	std::string m_response;
	DWORD m_status;
	std::string m_statusMessage;
	ULONG m_ref;

public:
	RequestCallback() : m_status(0), m_ref(0), m_responseReceived(false)
	{
	}

	~RequestCallback()
	{
	}

	HRESULT WaitForResponse(DWORD& status, std::string& response)
	{
		std::unique_lock<std::mutex> l(m_responseMutex);
		if (!m_responseReceived)
		{
			m_responseCV.wait(l);
		}

		if (!m_responseReceived)
		{
			return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		}

		status = m_status;
		response = m_response;
		return S_OK;
	}

	// IXMLHTTPRequest2Callback
	HRESULT CALLING_CONVENTION OnDataAvailable(IXMLHTTPRequest2* request, ISequentialStream* responseStream)
	{
		return S_OK;
	};

	HRESULT CALLING_CONVENTION OnError(IXMLHTTPRequest2 *request, HRESULT error)
	{
		return S_OK;
	}

	HRESULT CALLING_CONVENTION OnHeadersAvailable(IXMLHTTPRequest2 *request, DWORD status, const WCHAR *statusMessage)
	{
		m_status = status;
		m_statusMessage = wstring_to_utf8(statusMessage);
		return S_OK;
	}

	HRESULT CALLING_CONVENTION OnRedirect(IXMLHTTPRequest2 *request, const WCHAR *redirectUrl)
	{
		return S_OK;
	}
	
	HRESULT CALLING_CONVENTION OnResponseReceived(IXMLHTTPRequest2 *request, ISequentialStream *responseStream)
	{
		std::stringstream s;
		// Chunk the response in the stack rather than heap allocate.
		char buffer[1024];
		size_t bufferSize = sizeof(buffer) - 1; // Save a spot for the null terminator.
		HRESULT hr = S_OK;
		do
		{
			ULONG bytesRead = 0;
			hr = responseStream->Read(buffer, (ULONG)bufferSize, &bytesRead);
			if (FAILED(hr) || 0 == bytesRead)
			{
				break;
			}
			buffer[bytesRead] = 0;

			s << buffer;
		} while (S_OK == hr);

		if (SUCCEEDED(hr))
		{
			m_response = s.str();
		}

		std::unique_lock<std::mutex> l(m_responseMutex);
		m_responseReceived = true;
		m_responseCV.notify_one();

		return S_OK;
	}
};

winapp_http_client::winapp_http_client()
{
};

winapp_http_client::~winapp_http_client()
{
}

int
winapp_http_client::make_request(const std::string& uri, const std::string& verb, const std::map<std::string, std::string>* headers, const std::string& body, http_response& response, unsigned long timeoutMs) const
{
	HRESULT hr = S_OK;
	ComPtr<IXMLHTTPRequest2> request;
	ComPtr<RequestCallback> callback = Make<RequestCallback>();

#if _DURANGO
	DWORD context = CLSCTX_SERVER;
#else
	DWORD context = CLSCTX_INPROC;
#endif
	RETURN_HR_IF_FAILED(CoCreateInstance(__uuidof(FreeThreadedXMLHTTP60), nullptr, CLSCTX_INPROC, __uuidof(IXMLHTTPRequest2), reinterpret_cast<void**>(request.GetAddressOf())));

	if (nullptr != headers)
	{
		for (auto header : *headers)
		{	
			RETURN_HR_IF_FAILED(request->SetRequestHeader(utf8_to_wstring(header.first).c_str(), utf8_to_wstring(header.second).c_str()));
		}
	}

	ComPtr<IXMLHTTPRequest2Callback> xhrRequestCallback;
	RETURN_HR_IF_FAILED(callback.As<IXMLHTTPRequest2Callback>(&xhrRequestCallback));
	RETURN_HR_IF_FAILED(request->Open(utf8_to_wstring(verb).c_str(), utf8_to_wstring(uri).c_str(), xhrRequestCallback.Get(), nullptr, nullptr, nullptr, nullptr));

	RETURN_HR_IF_FAILED(request->SetProperty(XHR_PROP_TIMEOUT, timeoutMs));

	if (body.empty())
	{
		RETURN_HR_IF_FAILED(request->Send(nullptr, 0));
		RETURN_HR_IF_FAILED(callback->WaitForResponse((DWORD&)response.statusCode, response.body));
	}
	else
	{
		HttpRequestStream requestStream;
		RETURN_HR_IF_FAILED(requestStream.Open(body.c_str(), body.length()));
		RETURN_HR_IF_FAILED(request->Send(&requestStream, body.length()));
		RETURN_HR_IF_FAILED(callback->WaitForResponse((DWORD&)response.statusCode, response.body));
	}
	
	return S_OK;
}

}