#include "webserv/RequestParser.hpp"
#include <cctype>
#include <limits>
#include <sstream>

namespace
{
	const std::size_t kMaxHeaderSize = 16 * 1024;
	const std::size_t kMaxTargetSize = 8192;

	bool isTokenChar(char value)
	{
		const unsigned char c = static_cast<unsigned char>(value);

		if (std::isalnum(c) != 0)
			return (true);
		return (value == '!' || value == '#' || value == '$'
			|| value == '%' || value == '&' || value == '\''
			|| value == '*' || value == '+' || value == '-'
			|| value == '.' || value == '^' || value == '_'
			|| value == '`' || value == '|' || value == '~');
	}

	bool isValidToken(const std::string& value)
	{
		if (value.empty())
			return (false);
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (!isTokenChar(value[i]))
				return (false);
		}
		return (true);
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin;
		std::size_t end;

		begin = 0;
		while (begin < value.size()
			&& std::isspace(static_cast<unsigned char>(value[begin])) != 0)
			++begin;
		end = value.size();
		while (end > begin
			&& std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
			--end;
		return (value.substr(begin, end - begin));
	}

	std::string lowerString(const std::string& value)
	{
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
			result += static_cast<char>(
				std::tolower(static_cast<unsigned char>(value[i])));
		return (result);
	}

	bool parseSizeValue(const std::string& value, std::size_t& parsed)
	{
		const std::size_t max = std::numeric_limits<std::size_t>::max();

		if (value.empty())
			return (false);
		parsed = 0;
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (std::isdigit(static_cast<unsigned char>(value[i])) == 0)
				return (false);
			const std::size_t digit = static_cast<std::size_t>(value[i] - '0');
			if (parsed > (max - digit) / 10)
				return (false);
			parsed = parsed * 10 + digit;
		}
		return (true);
	}

	bool isHexDigit(char value)
	{
		return ((value >= '0' && value <= '9')
			|| (value >= 'a' && value <= 'f')
			|| (value >= 'A' && value <= 'F'));
	}

	int hexDigitValue(char value)
	{
		if (value >= '0' && value <= '9')
			return (value - '0');
		if (value >= 'a' && value <= 'f')
			return (value - 'a' + 10);
		if (value >= 'A' && value <= 'F')
			return (value - 'A' + 10);
		return (0);
	}

	bool parseChunkSizeLine(const std::string& line, std::size_t& parsed)
	{
		const std::size_t max = std::numeric_limits<std::size_t>::max();
		std::string sizePart;

		sizePart = line.substr(0, line.find(';'));
		sizePart = trim(sizePart);
		if (sizePart.empty())
			return (false);
		parsed = 0;
		for (std::size_t i = 0; i < sizePart.size(); ++i)
		{
			if (!isHexDigit(sizePart[i]))
				return (false);
			const std::size_t digit =
				static_cast<std::size_t>(hexDigitValue(sizePart[i]));
			if (parsed > (max - digit) / 16)
				return (false);
			parsed = parsed * 16 + digit;
		}
		return (true);
	}
}

namespace webserv
{
	RequestParser::RequestParser()
		: _state(PARSER_REQUEST_LINE), _consumedBytes(0), _errorStatus(0)
	{
	}

	RequestParser::Result RequestParser::parse(
		const std::string& input,
		HttpRequest& request)
	{
		const std::size_t headerEnd = input.find("\r\n\r\n");
		std::size_t expectedBodySize;
		bool hasChunkedBody;
		Result result;

		_consumedBytes = 0;
		_errorStatus = 0;
		if (headerEnd == std::string::npos)
		{
			_state = PARSER_HEADERS;
			if (input.size() > kMaxHeaderSize)
			{
				setError(HTTP_STATUS_BAD_REQUEST);
				request.setErrorStatus(_errorStatus);
				return (PARSE_ERROR);
			}
			return (PARSE_INCOMPLETE);
		}
		if (headerEnd > kMaxHeaderSize)
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			request.setErrorStatus(_errorStatus);
			return (PARSE_ERROR);
		}
		request.clear();
		_state = PARSER_REQUEST_LINE;
		const std::size_t requestLineEnd = input.find("\r\n");
		if (requestLineEnd == std::string::npos || requestLineEnd > headerEnd)
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			request.setErrorStatus(_errorStatus);
			return (PARSE_ERROR);
		}
		result = parseRequestLine(input.substr(0, requestLineEnd), request);
		if (result != PARSE_COMPLETE)
		{
			request.setErrorStatus(_errorStatus);
			return (result);
		}
		_state = PARSER_HEADERS;
		expectedBodySize = 0;
		hasChunkedBody = false;
		result = parseHeaders(
				input.substr(requestLineEnd + 2,
					headerEnd - (requestLineEnd + 2)),
				request,
				expectedBodySize,
				hasChunkedBody);
		if (result != PARSE_COMPLETE)
		{
			request.setErrorStatus(_errorStatus);
			return (result);
		}
		if (hasChunkedBody)
		{
			result = parseChunkedBody(input, headerEnd + 4, request);
			if (result != PARSE_COMPLETE)
			{
				request.setErrorStatus(_errorStatus);
				return (result);
			}
		}
		else if (request.hasContentLength())
		{
			const std::size_t bodyStart = headerEnd + 4;

			_state = PARSER_BODY_BY_LENGTH;
			if (input.size() - bodyStart < expectedBodySize)
				return (PARSE_INCOMPLETE);
			request.setBody(input.substr(bodyStart, expectedBodySize));
			_consumedBytes = bodyStart + expectedBodySize;
		}
		else
		{
			request.setBody("");
			_consumedBytes = headerEnd + 4;
		}
		_state = PARSER_COMPLETE;
		return (PARSE_COMPLETE);
	}

	RequestParserState RequestParser::state() const
	{
		return (_state);
	}

	std::size_t RequestParser::consumedBytes() const
	{
		return (_consumedBytes);
	}

	int RequestParser::errorStatus() const
	{
		return (_errorStatus);
	}

	void RequestParser::reset()
	{
		_state = PARSER_REQUEST_LINE;
		_consumedBytes = 0;
		_errorStatus = 0;
	}

	RequestParser::Result RequestParser::parseRequestLine(
		const std::string& line,
		HttpRequest& request)
	{
		std::istringstream stream(line);
		std::string method;
		std::string target;
		std::string version;
		std::string extra;
		std::size_t query;

		if (!(stream >> method >> target >> version) || (stream >> extra))
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			return (PARSE_ERROR);
		}
		if (!isValidToken(method))
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			return (PARSE_ERROR);
		}
		if (target.empty() || target.size() > kMaxTargetSize
			|| target[0] != '/')
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			return (PARSE_ERROR);
		}
		if (version != "HTTP/1.1")
		{
			setError(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
			return (PARSE_ERROR);
		}
		request.setMethodText(method);
		request.setMethod(parseHttpMethod(method));
		request.setRawTarget(target);
		query = target.find('?');
		if (query == std::string::npos)
		{
			request.setPath(target);
			request.setQuery("");
		}
		else
		{
			request.setPath(target.substr(0, query));
			request.setQuery(target.substr(query + 1));
		}
		request.setVersion(version);
		return (PARSE_COMPLETE);
	}

	RequestParser::Result RequestParser::parseHeaders(
		const std::string& headerBlock,
		HttpRequest& request,
		std::size_t& expectedBodySize,
		bool& hasChunkedBody)
	{
		std::size_t start;
		std::size_t end;
		bool hasHost;
		bool hasContentLength;
		bool hasTransferEncoding;
		std::size_t contentLength;

		start = 0;
		hasHost = false;
		hasContentLength = false;
		hasTransferEncoding = false;
		hasChunkedBody = false;
		contentLength = 0;
		while (start < headerBlock.size())
		{
			end = headerBlock.find("\r\n", start);
			if (end == std::string::npos)
				end = headerBlock.size();
			const std::string line = headerBlock.substr(start, end - start);
			const std::size_t colon = line.find(':');

			if (colon == std::string::npos)
			{
				setError(HTTP_STATUS_BAD_REQUEST);
				return (PARSE_ERROR);
			}
			const std::string name = trim(line.substr(0, colon));
			const std::string value = trim(line.substr(colon + 1));
			const std::string lowered = lowerString(name);

			if (!isValidToken(name))
			{
				setError(HTTP_STATUS_BAD_REQUEST);
				return (PARSE_ERROR);
			}
			if (lowered == "host")
			{
				hasHost = true;
				if (value.empty())
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
			}
			else if (lowered == "content-length")
			{
				std::size_t parsedLength;

				if (hasTransferEncoding)
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
				if (!parseSizeValue(value, parsedLength))
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
				if (hasContentLength && parsedLength != contentLength)
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
				hasContentLength = true;
				contentLength = parsedLength;
			}
			else if (lowered == "transfer-encoding")
			{
				if (hasContentLength)
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
				if (hasTransferEncoding)
				{
					setError(HTTP_STATUS_BAD_REQUEST);
					return (PARSE_ERROR);
				}
				hasTransferEncoding = true;
				if (trim(lowerString(value)) != "chunked")
				{
					setError(HTTP_STATUS_NOT_IMPLEMENTED);
					return (PARSE_ERROR);
				}
				request.setBodyFraming(HttpRequest::BODY_CHUNKED);
				hasChunkedBody = true;
			}
			request.addHeader(name, value);
			start = end;
			if (start < headerBlock.size())
				start += 2;
		}
		if (request.version() == "HTTP/1.1" && !hasHost)
		{
			setError(HTTP_STATUS_BAD_REQUEST);
			return (PARSE_ERROR);
		}
		if (hasContentLength)
		{
			request.setContentLength(contentLength);
			expectedBodySize = contentLength;
		}
		return (PARSE_COMPLETE);
	}

	RequestParser::Result RequestParser::parseChunkedBody(
		const std::string& input,
		std::size_t bodyStart,
		HttpRequest& request)
	{
		std::size_t position;
		std::string decodedBody;

		position = bodyStart;
		while (true)
		{
			std::size_t lineEnd;
			std::size_t chunkSize;

			_state = PARSER_BODY_CHUNK_SIZE;
			lineEnd = input.find("\r\n", position);
			if (lineEnd == std::string::npos)
			{
				request.setBody(decodedBody);
				return (PARSE_INCOMPLETE);
			}
			if (!parseChunkSizeLine(
					input.substr(position, lineEnd - position),
					chunkSize))
			{
				setError(HTTP_STATUS_BAD_REQUEST);
				return (PARSE_ERROR);
			}
			position = lineEnd + 2;
			if (chunkSize == 0)
			{
				_state = PARSER_BODY_CHUNK_TRAILERS;
				if (input.size() - position >= 2
					&& input.compare(position, 2, "\r\n") == 0)
				{
					request.setBody(decodedBody);
					_consumedBytes = position + 2;
					_state = PARSER_COMPLETE;
					return (PARSE_COMPLETE);
				}
				const std::size_t trailerEnd = input.find("\r\n\r\n", position);

				if (trailerEnd == std::string::npos)
				{
					request.setBody(decodedBody);
					return (PARSE_INCOMPLETE);
				}
				request.setBody(decodedBody);
				_consumedBytes = trailerEnd + 4;
				_state = PARSER_COMPLETE;
				return (PARSE_COMPLETE);
			}
			_state = PARSER_BODY_CHUNK_DATA;
			if (input.size() - position < chunkSize)
			{
				request.setBody(decodedBody);
				return (PARSE_INCOMPLETE);
			}
			decodedBody.append(input, position, chunkSize);
			position += chunkSize;
			_state = PARSER_BODY_CHUNK_CRLF;
			if (input.size() - position < 2)
			{
				request.setBody(decodedBody);
				return (PARSE_INCOMPLETE);
			}
			if (input.compare(position, 2, "\r\n") != 0)
			{
				setError(HTTP_STATUS_BAD_REQUEST);
				return (PARSE_ERROR);
			}
			position += 2;
		}
	}

	void RequestParser::setError(int status)
	{
		_state = PARSER_ERROR;
		_errorStatus = status;
		_consumedBytes = 0;
	}
}
