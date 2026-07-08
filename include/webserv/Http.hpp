#ifndef WEBSERV_HTTP_HPP
# define WEBSERV_HTTP_HPP

# include <string>

namespace webserv
{
	enum HttpMethod
	{
		HTTP_METHOD_GET,
		HTTP_METHOD_POST,
		HTTP_METHOD_DELETE,
		HTTP_METHOD_NOT_IMPLEMENTED
	};

	enum HttpStatus
	{
		HTTP_STATUS_OK = 200,
		HTTP_STATUS_CREATED = 201,
		HTTP_STATUS_NO_CONTENT = 204,
		HTTP_STATUS_MOVED_PERMANENTLY = 301,
		HTTP_STATUS_FOUND = 302,
		HTTP_STATUS_BAD_REQUEST = 400,
		HTTP_STATUS_FORBIDDEN = 403,
		HTTP_STATUS_NOT_FOUND = 404,
		HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
		HTTP_STATUS_PAYLOAD_TOO_LARGE = 413,
		HTTP_STATUS_URI_TOO_LONG = 414,
		HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
		HTTP_STATUS_NOT_IMPLEMENTED = 501,
		HTTP_STATUS_BAD_GATEWAY = 502,
		HTTP_STATUS_GATEWAY_TIMEOUT = 504,
		HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505
	};

	HttpMethod	parseHttpMethod(const std::string& method);
	const char*	httpMethodName(HttpMethod method);
	bool		isImplementedMethod(HttpMethod method);
	const char*	reasonPhrase(int statusCode);
	bool		isMandatoryStatusCode(int statusCode);
	bool		isErrorStatusCode(int statusCode);
}

#endif
