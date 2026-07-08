#include "webserv/ResponseBuilder.hpp"
#include "webserv/ErrorPageHandler.hpp"
#include "webserv/Http.hpp"

namespace
{
	std::string joinMethods(const std::vector<std::string>& methods)
	{
		std::string result;

		for (std::size_t i = 0; i < methods.size(); ++i)
		{
			if (i != 0)
				result += ", ";
			result += methods[i];
		}
		return (result);
	}

	int normalizeStatus(int statusCode)
	{
		if (webserv::reasonPhrase(statusCode) == std::string("Unknown"))
			return (webserv::HTTP_STATUS_INTERNAL_SERVER_ERROR);
		return (statusCode);
	}
}

namespace webserv
{
	HttpResponse ResponseBuilder::text(
		int statusCode,
		const std::string& body,
		const std::string& contentType)
	{
		HttpResponse response;
		const int normalizedStatus = normalizeStatus(statusCode);

		response.setStatusCode(normalizedStatus);
		response.setHeader("Content-Type", contentType);
		if (normalizedStatus != HTTP_STATUS_NO_CONTENT)
			response.setBody(body);
		response.setConnectionClose(true);
		return (response);
	}

	HttpResponse ResponseBuilder::error(int statusCode)
	{
		HttpResponse response;
		const int normalizedStatus = normalizeStatus(statusCode);

		response.setStatusCode(normalizedStatus);
		response.setHeader("Content-Type", "text/html");
		response.setBody(ErrorPageHandler::defaultErrorPage(normalizedStatus));
		response.setConnectionClose(true);
		return (response);
	}

	HttpResponse ResponseBuilder::redirect(
		int statusCode,
		const std::string& location)
	{
		HttpResponse response;
		const int normalizedStatus = normalizeStatus(statusCode);

		response.setStatusCode(normalizedStatus);
		response.setHeader("Location", location);
		response.setHeader("Content-Type", "text/plain");
		response.setBody(std::string(reasonPhrase(normalizedStatus)) + "\n");
		response.setConnectionClose(true);
		return (response);
	}

	HttpResponse ResponseBuilder::methodNotAllowed(
		const std::vector<std::string>& allowedMethods)
	{
		HttpResponse response = error(HTTP_STATUS_METHOD_NOT_ALLOWED);

		response.setHeader("Allow", joinMethods(allowedMethods));
		return (response);
	}
}
