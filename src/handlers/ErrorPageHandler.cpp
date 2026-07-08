#include "webserv/ErrorPageHandler.hpp"
#include "webserv/Http.hpp"
#include "webserv/MimeTypes.hpp"
#include "webserv/PathPolicy.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	const std::size_t kReadBufferSize = 8192;

	bool isRegularFileMode(mode_t mode)
	{
		return (S_ISREG(mode));
	}
}

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

	bool ErrorPageHandler::loadCustomErrorPage(
		int statusCode,
		const std::map<int, std::string>& errorPages,
		const std::string& root,
		std::string& body,
		std::string& contentType)
	{
		std::map<int, std::string>::const_iterator it;
		PathResolution resolved;
		struct stat info;
		int fd;
		char buffer[kReadBufferSize];

		it = errorPages.find(statusCode);
		if (it == errorPages.end())
			return (false);
		resolved = resolveUriPath(it->second, "/", root);
		if (!resolved.ok)
			return (false);
		if (stat(resolved.filesystemPath.c_str(), &info) != 0)
			return (false);
		if (!isRegularFileMode(info.st_mode))
			return (false);
		if (access(resolved.filesystemPath.c_str(), R_OK) != 0)
			return (false);
		fd = open(resolved.filesystemPath.c_str(), O_RDONLY);
		if (fd < 0)
			return (false);
		body.clear();
		while (true)
		{
			const ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

			if (bytesRead > 0)
			{
				body.append(buffer, static_cast<std::size_t>(bytesRead));
				continue;
			}
			if (bytesRead == 0)
				break;
			if (errno == EINTR)
				continue;
			close(fd);
			body.clear();
			return (false);
		}
		close(fd);
		contentType = mimeTypeForPath(resolved.filesystemPath);
		return (true);
	}
}
