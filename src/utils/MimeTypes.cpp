#include "webserv/MimeTypes.hpp"

namespace
{
	std::string extensionForPath(const std::string& path)
	{
		const std::size_t slash = path.find_last_of('/');
		const std::size_t dot = path.find_last_of('.');

		if (dot == std::string::npos)
			return ("");
		if (slash != std::string::npos && dot < slash)
			return ("");
		return (path.substr(dot));
	}
}

namespace webserv
{
	std::string mimeTypeForPath(const std::string& path)
	{
		const std::string extension = extensionForPath(path);

		if (extension == ".html" || extension == ".htm")
			return ("text/html");
		if (extension == ".css")
			return ("text/css");
		if (extension == ".js")
			return ("application/javascript");
		if (extension == ".txt")
			return ("text/plain");
		if (extension == ".jpg" || extension == ".jpeg")
			return ("image/jpeg");
		if (extension == ".png")
			return ("image/png");
		if (extension == ".gif")
			return ("image/gif");
		if (extension == ".ico")
			return ("image/x-icon");
		if (extension == ".svg")
			return ("image/svg+xml");
		return ("application/octet-stream");
	}
}
