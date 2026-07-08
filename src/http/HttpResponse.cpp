#include "webserv/HttpResponse.hpp"
#include "webserv/Http.hpp"
#include <cctype>
#include <sstream>

namespace
{
	std::string lowerString(const std::string& value)
	{
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
			result += static_cast<char>(
				std::tolower(static_cast<unsigned char>(value[i])));
		return (result);
	}
}

namespace webserv
{
	HttpResponse::HttpResponse()
		: _version("HTTP/1.1"),
		  _statusCode(HTTP_STATUS_OK),
		  _reasonPhrase(webserv::reasonPhrase(HTTP_STATUS_OK)),
		  _headers(),
		  _body(),
		  _connectionClose(true)
	{
	}

	void HttpResponse::setVersion(const std::string& version)
	{
		_version = version;
	}

	void HttpResponse::setStatusCode(int statusCode)
	{
		_statusCode = statusCode;
		_reasonPhrase = webserv::reasonPhrase(statusCode);
	}

	void HttpResponse::setReasonPhrase(const std::string& reasonPhraseValue)
	{
		_reasonPhrase = reasonPhraseValue;
	}

	void HttpResponse::setHeader(
		const std::string& name,
		const std::string& value)
	{
		const std::string lowered = lowerString(name);

		for (std::size_t i = 0; i < _headers.size(); ++i)
		{
			if (lowerString(_headers[i].first) == lowered)
			{
				_headers[i].second = value;
				return;
			}
		}
		_headers.push_back(Header(name, value));
	}

	void HttpResponse::setBody(const std::string& body)
	{
		_body = body;
	}

	void HttpResponse::setConnectionClose(bool closeConnection)
	{
		_connectionClose = closeConnection;
	}

	const std::string& HttpResponse::version() const
	{
		return (_version);
	}

	int HttpResponse::statusCode() const
	{
		return (_statusCode);
	}

	const std::string& HttpResponse::reasonPhrase() const
	{
		return (_reasonPhrase);
	}

	const std::string& HttpResponse::body() const
	{
		return (_body);
	}

	bool HttpResponse::connectionClose() const
	{
		return (_connectionClose);
	}

	std::string HttpResponse::serialize() const
	{
		std::ostringstream stream;
		const bool dropBody = statusMustNotHaveBody();
		const std::size_t bodySize = dropBody ? 0 : _body.size();

		stream << _version << " " << _statusCode << " "
			   << _reasonPhrase << "\r\n";
		for (std::size_t i = 0; i < _headers.size(); ++i)
			stream << _headers[i].first << ": " << _headers[i].second
				   << "\r\n";
		if (!hasHeader("Content-Length"))
			stream << "Content-Length: " << bodySize << "\r\n";
		if (!hasHeader("Connection"))
			stream << "Connection: "
				   << (_connectionClose ? "close" : "keep-alive")
				   << "\r\n";
		stream << "\r\n";
		if (!dropBody)
			stream << _body;
		return (stream.str());
	}

	bool HttpResponse::hasHeader(const std::string& name) const
	{
		const std::string lowered = lowerString(name);

		for (std::size_t i = 0; i < _headers.size(); ++i)
		{
			if (lowerString(_headers[i].first) == lowered)
				return (true);
		}
		return (false);
	}

	bool HttpResponse::statusMustNotHaveBody() const
	{
		return (_statusCode == HTTP_STATUS_NO_CONTENT);
	}
}
