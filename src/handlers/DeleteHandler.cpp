#include "webserv/DeleteHandler.hpp"
#include "webserv/Http.hpp"
#include "webserv/ResponseBuilder.hpp"
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

namespace
{
	bool isDirectoryMode(mode_t mode)
	{
		return (S_ISDIR(mode));
	}

	bool isRegularFileMode(mode_t mode)
	{
		return (S_ISREG(mode));
	}

	webserv::HttpResponse errorForRemoveFailure()
	{
		if (errno == ENOENT)
			return (webserv::ResponseBuilder::error(
					webserv::HTTP_STATUS_NOT_FOUND));
		if (errno == EACCES || errno == EPERM)
			return (webserv::ResponseBuilder::error(
					webserv::HTTP_STATUS_FORBIDDEN));
		return (webserv::ResponseBuilder::error(
				webserv::HTTP_STATUS_INTERNAL_SERVER_ERROR));
	}
}

namespace webserv
{
	DeleteHandler::DeleteHandler()
	{
	}

	HttpResponse DeleteHandler::handleDelete(
		const std::string& filesystemPath,
		const std::string& relativePath) const
	{
		struct stat info;

		if (relativePath.empty())
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (stat(filesystemPath.c_str(), &info) != 0)
		{
			if (errno == ENOENT)
				return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
			if (errno == EACCES || errno == EPERM)
				return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
			return (ResponseBuilder::error(HTTP_STATUS_INTERNAL_SERVER_ERROR));
		}
		if (isDirectoryMode(info.st_mode))
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (!isRegularFileMode(info.st_mode))
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		errno = 0;
		if (std::remove(filesystemPath.c_str()) != 0)
			return (errorForRemoveFailure());
		return (ResponseBuilder::text(
				HTTP_STATUS_NO_CONTENT,
				"",
				"text/plain"));
	}
}
