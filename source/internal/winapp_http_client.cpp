#include "winapp_http_client.h"
#include "common.h"
#include <mutex>
#include <condition_variable>

#if _DURANGO
#include <ixmlhttprequest2.h>
#else
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
	HRESULT Read(void *buffer, unsigned long bufferSize, unsigned long *bytesRead)
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
	HRESULT Write(const void *, unsigned long, unsigned long *) { return E_NOTIMPL; }

	// IDispatch interface is required but not used in this context.  The methods are empty.
	HRESULT GetTypeInfoCount(unsigned int FAR*) { return E_NOTIMPL; }
	HRESULT GetTypeInfo(unsigned int, LCID, ITypeInfo FAR* FAR*) { return E_NOTIMPL; }
	HRESULT GetIDsOfNames(REFIID, OLECHAR FAR* FAR*, unsigned int, LCID, DISPID FAR*) { return DISP_E_UNKNOWNNAME; }
	HRESULT Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS FAR*, VARIANT FAR*, EXCEPINFO FAR*, unsigned int FAR*) { return S_OK; }

	// Methods created for simplicity when creating and passing along the buffer
	HRESULT Open(const void *buffer, size_t bufferSize)
	{
		AllocateInternalBuffer(bufferSize);

		memcpy(m_buffer, buffer, bufferSize);

		return S_OK;
	}
	size_t Size() const { return m_bufferSize; }
private:
	void AllocateInternalBuffer(size_t size)
	{
		if (m_buffer != nullptr)
		{
			FreeInternalBuffer();
		}

		m_bufferSize = size;
		m_buffer = new unsigned char[size];
	}
	void FreeInternalBuffer()
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

	HRESULT WaitForResponse(ULONG timeoutMs, DWORD& status, std::string& response)
	{
		std::unique_lock<std::mutex> l(m_responseMutex);
		if (!m_responseReceived)
		{
			m_responseCV.wait_for(l, std::chrono::milliseconds(timeoutMs));
		}

		if (!m_responseReceived)
		{
			return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		}

		status = m_status;
		response = m_response;
	}

	// IXMLHTTPRequest2Callback
	HRESULT OnDataAvailable(IXMLHTTPRequest2* request, ISequentialStream* responseStream)
	{
		return S_OK;
	};

	HRESULT OnError(IXMLHTTPRequest2 *request, HRESULT error)
	{
		return S_OK;
	}

	HRESULT OnHeadersAvailable(IXMLHTTPRequest2 *request, DWORD status, const WCHAR *statusMessage)
	{
		m_status = status;
		m_statusMessage = wstring_to_utf8(statusMessage);
		return S_OK;
	}

	HRESULT OnRedirect(IXMLHTTPRequest2 *request, const WCHAR *redirectUrl)
	{
		return S_OK;
	}
	
	HRESULT OnResponseReceived(IXMLHTTPRequest2 *request, ISequentialStream *responseStream)
	{
		std::stringstream s;
		char buffer[1024];
		size_t bufferSize = sizeof(buffer) - 1; // Save space for the null character
		HRESULT hr = S_OK;
		do
		{
			ULONG bytesRead = 0;
			hr = responseStream->Read(buffer, bufferSize, &bytesRead);
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
		
		return hr;
	}
};

winapp_http_client::winapp_http_client()
{
};

winapp_http_client::~winapp_http_client()
{
}

int
winapp_http_client::make_request(const std::string& uri, const std::string& verb, const std::map<std::string, std::string>& headers, const std::string& body, http_response& response, unsigned long timeoutMs = 5000) const
{
	HRESULT hr = S_OK;
	ComPtr<IXMLHTTPRequest2> request;
	RequestCallback callback;

	DWORD context = CLSCTX_INPROC;
#if _DURANGO
	DWORD context = CLSCTX_INPROC;
#else
	DWORD context = CLSCTX_SERVER;
#endif
	RETURN_HR_IF_FAILED(CoCreateInstance(__uuidof(FreeThreadedXMLHTTP60), nullptr, CLSCTX_INPROC, __uuidof(IXMLHTTPRequest2), reinterpret_cast<void**>(request.GetAddressOf())));

	for (auto header : headers)
	{
		RETURN_HR_IF_FAILED(request->SetRequestHeader(utf8_to_wstring(header.first).c_str(), utf8_to_wstring(header.second).c_str()));
	}

	RETURN_HR_IF_FAILED(request->Open(utf8_to_wstring(verb).c_str(), utf8_to_wstring(uri).c_str(), &callback, nullptr, nullptr, nullptr, nullptr));

	if (body.empty())
	{
		RETURN_HR_IF_FAILED(request->Send(nullptr, 0));
	}
	else
	{
		ComPtr<IStream> bodyStream;
		raStream->Write(
			/*
		auto writer = ref new DataWriter(raStream->(0));
		writer->WriteString(StringReference(utf8_to_wstring(body).c_str()));*/

		
		bodyStream = raStream->GetInputStreamAt(0);
	}

	RETURN_HR_IF_FAILED(callback.WaitForResponse(timeoutMs, (DWORD&)response.statusCode, response.body));
	return hr;
}

}