#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "websocket.h"
#include "common.h"

#include <ppltasks.h>
#include <sstream>
#include <queue>
#include <condition_variable>

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Networking::Sockets;

class semaphore
{
private:
	std::mutex mutex_;
	std::condition_variable condition_;
	unsigned long count_ = 0; // Initialized as locked.

public:
	void notify()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		++count_;
		condition_.notify_one();
	}

	void wait()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		while (!count_) // Handle spurious wake-ups.
		{
			condition_.wait(lock);
		}
		--count_;
	}

	bool try_wait()
	{
		std::unique_lock<decltype(mutex_)> lock(mutex_);
		if (count_)
		{
			--count_;
			return true;
		}
		return false;
	}
};

class winapp_websocket : public websocket
{
private: std::map<std::string, std::string> m_headers;
public:
	winapp_websocket() : m_connected(false), m_closeCode(0)
	{
	}

	int add_header(const std::string& key, const std::string& value)
	{
		m_headers[key] = value;
		return 0;
	}

	void on_close(IWebSocket^ sender, WebSocketClosedEventArgs^ args)
	{
		// Mark the close code and reason.
		m_closeCode = args->Code;
		m_closeReason = wstring_to_utf8(args->Reason->Data());

		// Notify message processing loop.
		std::lock_guard<std::mutex> autoLock(m_messagesMutex);
		m_connected = false;
		m_messagesReadyCV.notify_one();
	}

	void on_message(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
	{
		if (args->MessageType == SocketMessageType::Utf8)
		{
			DataReader^ reader = args->GetDataReader();
			reader->UnicodeEncoding = UnicodeEncoding::Utf8;
			String^ message = reader->ReadString(reader->UnconsumedBufferLength);

			std::lock_guard<std::mutex> lock(m_messagesMutex);
			m_messages.push(wstring_to_utf8(message->Data()));
			m_messagesReadyCV.notify_one();
		}
	}

	int open(const std::string& uri, const on_ws_connect onConnect, const on_ws_message onMessage, const on_ws_error onError, const on_ws_close onClose)
	{
		int result = 0;
		m_ws = ref new MessageWebSocket();
		m_ws->Control->MessageType = SocketMessageType::Utf8;

		// Add any headers.
		for (auto header : m_headers)
		{
			std::wstring keyWS = utf8_to_wstring(header.first);
			std::wstring valueWS = utf8_to_wstring(header.second);
			m_ws->SetRequestHeader(StringReference(keyWS.c_str()), StringReference(valueWS.c_str()));
		}

		// Add closed and received handlers.
		std::function<void(IWebSocket^, WebSocketClosedEventArgs^)> onWsClose = std::bind(&winapp_websocket::on_close, this, std::placeholders::_1, std::placeholders::_2);
		m_ws->Closed += ref new TypedEventHandler<IWebSocket^, WebSocketClosedEventArgs^>(onWsClose);
		std::function<void(MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^)> onWsMessage = std::bind(&winapp_websocket::on_message, this, std::placeholders::_1, std::placeholders::_2);
		m_ws->MessageReceived += ref new TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(onWsMessage);
		
		// Convert uri string to Windows Uri object.
		std::wstring uriWS = utf8_to_wstring(uri);
		Uri^ uriRef = ref new Uri(StringReference(uriWS.c_str()));

		// Connect asynchronously and then notify
		create_task(m_ws->ConnectAsync(uriRef)).then([&](task<void> prevTask)
		{
			std::lock_guard<std::mutex> autoLock(m_messagesMutex);
			try
			{
				prevTask.get();
				m_connected = true;
			}
			catch (Exception^ ex)
			{
				result = ex->HResult;
			}

			m_openSemaphore.notify();
		});

		// Wait for connection notification.
		m_openSemaphore.wait();

		// Call error handler if connection was not successful.
		if (0 != result || !m_connected)
		{
			if (onError)
			{
				if (m_closeCode)
				{
					onError(*this, m_closeCode, m_closeReason);
				}
				else
				{
					std::string message = "Connection failed.";
					onError(*this, 4000, message);
				}
			}

			return result;
		}

		// Connection was successful.
		if (onConnect)
		{
			std::string connectMessage = "Connected to: " + uri;
			onConnect(*this, connectMessage);
		}

		// Process messages until the socket is closed;
		for (;;)
		{
			// See if we're no longer connected, if so call the onClose callback.
			if (!m_connected)
			{
				if (onClose)
				{
					onClose(*this, m_closeCode, m_closeReason);
				}

				break;
			}

			// Check for messages
			std::unique_lock<std::mutex> messagesLock(m_messagesMutex);
			if (m_messages.empty())
			{	
				// Wait for a message
				m_messagesReadyCV.wait(messagesLock);
			}

			std::string message = m_messages.front();
			m_messages.pop();
			messagesLock.unlock();

			if (onMessage)
			{
				onMessage(*this, message);
			}
		}

		return result;
	}

	int send(const std::string& message)
	{
		if (!m_connected)
		{
			return E_ABORT;
		}

		DataWriter writer(m_ws->OutputStream);
		writer.WriteString(StringReference(utf8_to_wstring(message).c_str()));
		try
		{
			create_task(writer.StoreAsync()).wait();
			writer.DetachStream();
		}
		catch (Exception^ ex)
		{
			return ex->HResult;
		}

		return 0;
	}

	int read(std::string& message)
	{
		if (!m_connected)
		{
			return E_ABORT;
		}

		std::unique_lock<std::mutex> messagesLock(m_messagesMutex);
		if (m_messages.empty())
		{
			// Wait for a message.
			m_messagesReadyCV.wait(messagesLock);

			if (!m_connected && m_messages.empty())
			{
				// Incorrectly consumed close notification, pass it on.
				m_messagesReadyCV.notify_one();
				return E_ABORT;
			}
		}
		
		message = m_messages.front();
		m_messages.pop();

		return 0;
	}

	void close()
	{
		if (m_connected)
		{
			m_ws->Close(1000, nullptr);
		}
	}

private:
	MessageWebSocket ^ m_ws;
	bool m_connected;
	unsigned short m_closeCode;
	std::string m_closeReason;
	std::mutex m_messagesMutex;
	std::condition_variable m_messagesReadyCV;
	semaphore m_openSemaphore;
	std::queue<std::string> m_messages;
};


std::unique_ptr<websocket>
websocket_factory::make_websocket()
{
	return std::unique_ptr<websocket>(new winapp_websocket());
}

extern "C" {

	int create_websocket(websocket_handle* handlePtr)
	{
		if (nullptr == handlePtr)
		{
			return 1;
		}

		std::unique_ptr<websocket> handle = websocket_factory::make_websocket();
		*handlePtr = handle.release();
		return 0;
	}

	int add_header(websocket_handle handle, const char* key, const char* value)
	{
		websocket* client = reinterpret_cast<websocket*>(handle);
		return client->add_header(key, value);
	}

	int open_websocket(websocket_handle handle, const char* uri, const c_on_ws_connect onConnect, const c_on_ws_message onMessage, const c_on_ws_error onError, const c_on_ws_close onClose)
	{
		websocket* client = reinterpret_cast<websocket*>(handle);


		auto connectHandler = [&](const websocket& socket, const std::string& connectMessage)
		{
			if (onConnect)
			{
				onConnect((void*)&socket, connectMessage.c_str(), (unsigned int)connectMessage.length());
			}
		};

		auto messageHandler = [&](const websocket& socket, const std::string& message)
		{
			if (onMessage)
			{
				onMessage((void*)&socket, message.c_str(), (unsigned int)message.length());
			}
		};

		auto errorHandler = [&](const websocket& socket, unsigned short code, const std::string& error)
		{
			if (onError)
			{
				onError((void*)&socket, code, error.c_str(), (unsigned int)error.length());
			}
		};

		auto closeHandler = [&](const websocket& socket, unsigned short code, const std::string& reason)
		{
			if (onClose)
			{
				onClose((void*)&socket, code, reason.c_str(), (unsigned int)reason.length());
			}
		};

		return client->open(uri, connectHandler, messageHandler, errorHandler, closeHandler);
	}

	int write_websocket(websocket_handle handle, const char* message)
	{
		websocket* client = reinterpret_cast<websocket*>(handle);
		return client->send(message);
	}

	int read_websocket(websocket_handle handle, c_on_ws_message onMessage)
	{
		if (nullptr == handle || nullptr == onMessage)
		{
			return 1;
		}

		websocket* client = reinterpret_cast<websocket*>(handle);
		std::string message;
		int error = client->read(message);
		if (error)
		{
			return error;
		}

		return 0;
	}

	int close_websocket(websocket_handle handle)
	{
		websocket* client = reinterpret_cast<websocket*>(handle);
		client->close();
		delete client;
		return 0;
	}
}