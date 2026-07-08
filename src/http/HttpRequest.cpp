#include "webserv/HttpRequest.hpp"
#include <cctype>

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
	HttpRequest::HttpRequest()
	{
		clear();
	}

	void HttpRequest::clear()
	{
		_methodText = "";
		_method = HTTP_METHOD_NOT_IMPLEMENTED;
		_rawTarget = "";
		_path = "";
		_query = "";
		_version = "";
		_headers.clear();
		_body = "";
		_bodyFraming = BODY_NONE;
		_hasContentLength = false;
		_contentLength = 0;
		_errorStatus = 0;
	}

	void HttpRequest::setMethodText(const std::string& methodText)
	{
		_methodText = methodText;
	}

	void HttpRequest::setMethod(HttpMethod method)
	{
		_method = method;
	}

	void HttpRequest::setRawTarget(const std::string& rawTarget)
	{
		_rawTarget = rawTarget;
	}

	void HttpRequest::setPath(const std::string& path)
	{
		_path = path;
	}

	void HttpRequest::setQuery(const std::string& query)
	{
		_query = query;
	}

	void HttpRequest::setVersion(const std::string& version)
	{
		_version = version;
	}

	void HttpRequest::addHeader(const std::string& name, const std::string& value)
	{
		const std::string lowered = lowerString(name);

		if (_headers.find(lowered) == _headers.end())
			_headers[lowered] = value;
		else
			_headers[lowered] += ", " + value;
	}

	void HttpRequest::setBody(const std::string& body)
	{
		_body = body;
	}

	void HttpRequest::setBodyFraming(BodyFraming framing)
	{
		_bodyFraming = framing;
	}

	void HttpRequest::setContentLength(std::size_t contentLength)
	{
		_hasContentLength = true;
		_contentLength = contentLength;
		_bodyFraming = BODY_CONTENT_LENGTH;
	}

	void HttpRequest::setErrorStatus(int status)
	{
		_errorStatus = status;
	}

	const std::string& HttpRequest::methodText() const
	{
		return (_methodText);
	}

	HttpMethod HttpRequest::method() const
	{
		return (_method);
	}

	const std::string& HttpRequest::rawTarget() const
	{
		return (_rawTarget);
	}

	const std::string& HttpRequest::path() const
	{
		return (_path);
	}

	const std::string& HttpRequest::query() const
	{
		return (_query);
	}

	const std::string& HttpRequest::version() const
	{
		return (_version);
	}

	const std::string& HttpRequest::body() const
	{
		return (_body);
	}

	HttpRequest::BodyFraming HttpRequest::bodyFraming() const
	{
		return (_bodyFraming);
	}

	bool HttpRequest::hasContentLength() const
	{
		return (_hasContentLength);
	}

	std::size_t HttpRequest::contentLength() const
	{
		return (_contentLength);
	}

	bool HttpRequest::hasHeader(const std::string& name) const
	{
		return (_headers.find(lowerString(name)) != _headers.end());
	}

	std::string HttpRequest::header(const std::string& name) const
	{
		std::map<std::string, std::string>::const_iterator it;

		it = _headers.find(lowerString(name));
		if (it == _headers.end())
			return ("");
		return (it->second);
	}

	int HttpRequest::errorStatus() const
	{
		return (_errorStatus);
	}
}
