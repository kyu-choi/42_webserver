#ifndef WEBSERV_RESPONSE_BUILDER_HPP
# define WEBSERV_RESPONSE_BUILDER_HPP

# include "webserv/HttpResponse.hpp"
# include <string>
# include <vector>

namespace webserv
{
	class ResponseBuilder
	{
	public:
		static HttpResponse	text(
								int statusCode,
								const std::string& body,
								const std::string& contentType);
		static HttpResponse	error(int statusCode);
		static HttpResponse	redirect(int statusCode, const std::string& location);
		static HttpResponse	methodNotAllowed(
								const std::vector<std::string>& allowedMethods);

	private:
		ResponseBuilder();
	};
}

#endif
