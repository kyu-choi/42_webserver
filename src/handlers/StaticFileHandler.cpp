#include "webserv/StaticFileHandler.hpp"
#include "webserv/MimeTypes.hpp"
#include "webserv/PathPolicy.hpp"
#include "webserv/ResponseBuilder.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	const std::size_t kReadBufferSize = 8192;

	bool isDirectoryMode(mode_t mode)
	{
		return (S_ISDIR(mode));
	}

	bool isRegularFileMode(mode_t mode)
	{
		return (S_ISREG(mode));
	}
}

namespace webserv
{
	StaticFileHandler::StaticFileHandler(const std::string& root)
		: _root(root)
	{
	}

	HttpResponse StaticFileHandler::handleGet(const HttpRequest& request) const
	{
		PathResolution resolved;

		if (request.method() != HTTP_METHOD_GET)
			return (ResponseBuilder::error(HTTP_STATUS_METHOD_NOT_ALLOWED));
		resolved = resolveUriPath(request.rawTarget(), "/", _root);
		if (!resolved.ok)
		{
			if (resolved.error == PATH_POLICY_TRAVERSAL)
				return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
			if (resolved.error == PATH_POLICY_BAD_URI
				|| resolved.error == PATH_POLICY_BAD_ESCAPE)
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
		}
		if (resolved.uriPath == "/")
			resolved.filesystemPath = _root + "/index.html";
		return (buildFileResponse(resolved.filesystemPath));
	}

	HttpResponse StaticFileHandler::buildFileResponse(const std::string& path) const
	{
		struct stat	info;
		int			fd;
		std::string	body;
		char		buffer[kReadBufferSize];

		if (stat(path.c_str(), &info) != 0)
			return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
		if (isDirectoryMode(info.st_mode))
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (!isRegularFileMode(info.st_mode))
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (access(path.c_str(), R_OK) != 0)
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0)
		{
			if (errno == EACCES)
				return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
			return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
		}
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
			return (ResponseBuilder::error(HTTP_STATUS_INTERNAL_SERVER_ERROR));
		}
		close(fd);
		return (ResponseBuilder::text(
				HTTP_STATUS_OK,
				body,
				mimeTypeForPath(path)));
	}
}
