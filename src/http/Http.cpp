#include "webserv/Http.hpp"

namespace webserv
{
	HttpMethod parseHttpMethod(const std::string& method)
	{
		if (method == "GET")
			return (HTTP_METHOD_GET);
		if (method == "POST")
			return (HTTP_METHOD_POST);
		if (method == "DELETE")
			return (HTTP_METHOD_DELETE);
		return (HTTP_METHOD_NOT_IMPLEMENTED);
	}

	const char* httpMethodName(HttpMethod method)
	{
		switch (method)
		{
			case HTTP_METHOD_GET:
				return ("GET");
			case HTTP_METHOD_POST:
				return ("POST");
			case HTTP_METHOD_DELETE:
				return ("DELETE");
			case HTTP_METHOD_NOT_IMPLEMENTED:
				return ("NOT_IMPLEMENTED");
		}
		return ("NOT_IMPLEMENTED");
	}

	bool isImplementedMethod(HttpMethod method)
	{
		return (method == HTTP_METHOD_GET
			|| method == HTTP_METHOD_POST
			|| method == HTTP_METHOD_DELETE);
	}

	const char* reasonPhrase(int statusCode)
	{
		switch (statusCode)
		{
			case HTTP_STATUS_OK:
				return ("OK");
			case HTTP_STATUS_CREATED:
				return ("Created");
			case HTTP_STATUS_NO_CONTENT:
				return ("No Content");
			case HTTP_STATUS_MOVED_PERMANENTLY:
				return ("Moved Permanently");
			case HTTP_STATUS_FOUND:
				return ("Found");
			case HTTP_STATUS_BAD_REQUEST:
				return ("Bad Request");
			case HTTP_STATUS_FORBIDDEN:
				return ("Forbidden");
			case HTTP_STATUS_NOT_FOUND:
				return ("Not Found");
			case HTTP_STATUS_METHOD_NOT_ALLOWED:
				return ("Method Not Allowed");
			case HTTP_STATUS_PAYLOAD_TOO_LARGE:
				return ("Payload Too Large");
			case HTTP_STATUS_URI_TOO_LONG:
				return ("URI Too Long");
			case HTTP_STATUS_INTERNAL_SERVER_ERROR:
				return ("Internal Server Error");
			case HTTP_STATUS_NOT_IMPLEMENTED:
				return ("Not Implemented");
			case HTTP_STATUS_BAD_GATEWAY:
				return ("Bad Gateway");
			case HTTP_STATUS_GATEWAY_TIMEOUT:
				return ("Gateway Timeout");
			case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
				return ("HTTP Version Not Supported");
		}
		return ("Unknown");
	}

	bool isMandatoryStatusCode(int statusCode)
	{
		return (statusCode == HTTP_STATUS_OK
			|| statusCode == HTTP_STATUS_CREATED
			|| statusCode == HTTP_STATUS_NO_CONTENT
			|| statusCode == HTTP_STATUS_MOVED_PERMANENTLY
			|| statusCode == HTTP_STATUS_FOUND
			|| statusCode == HTTP_STATUS_BAD_REQUEST
			|| statusCode == HTTP_STATUS_FORBIDDEN
			|| statusCode == HTTP_STATUS_NOT_FOUND
			|| statusCode == HTTP_STATUS_METHOD_NOT_ALLOWED
			|| statusCode == HTTP_STATUS_PAYLOAD_TOO_LARGE
			|| statusCode == HTTP_STATUS_URI_TOO_LONG
			|| statusCode == HTTP_STATUS_INTERNAL_SERVER_ERROR
			|| statusCode == HTTP_STATUS_NOT_IMPLEMENTED
			|| statusCode == HTTP_STATUS_BAD_GATEWAY
			|| statusCode == HTTP_STATUS_GATEWAY_TIMEOUT
			|| statusCode == HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
	}

	bool isErrorStatusCode(int statusCode)
	{
		return (statusCode >= 400);
	}
}
