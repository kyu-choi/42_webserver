#include "webserv/ErrorPageHandler.hpp"
#include "webserv/Http.hpp"
#include <sstream>

namespace webserv
{
	std::string ErrorPageHandler::defaultErrorPage(int statusCode)
	{
		std::ostringstream stream;

		stream << "<!doctype html>\n"
			   << "<html>\n"
			   << "<head><title>" << statusCode << " "
			   << reasonPhrase(statusCode) << "</title></head>\n"
			   << "<body><h1>" << statusCode << " "
			   << reasonPhrase(statusCode) << "</h1></body>\n"
			   << "</html>\n";
		return (stream.str());
	}
}
