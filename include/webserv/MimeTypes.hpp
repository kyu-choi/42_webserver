#ifndef WEBSERV_MIME_TYPES_HPP
# define WEBSERV_MIME_TYPES_HPP

# include <string>

namespace webserv
{
	std::string	mimeTypeForPath(const std::string& path);
}

#endif
