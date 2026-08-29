#include "webserv/StaticFileHandler.hpp"
#include "webserv/MimeTypes.hpp"
#include "webserv/PathPolicy.hpp"
#include "webserv/ResponseBuilder.hpp"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
	const std::size_t kReadBufferSize = 8192;

	struct DirectoryEntry
	{
		std::string	name;
		bool		isDirectory;

		DirectoryEntry(const std::string& nameValue, bool isDirectoryValue)
			: name(nameValue), isDirectory(isDirectoryValue)
		{
		}
	};

	bool isDirectoryMode(mode_t mode)
	{
		return (S_ISDIR(mode));
	}

	bool isRegularFileMode(mode_t mode)
	{
		return (S_ISREG(mode));
	}

	std::string joinPath(const std::string& directory, const std::string& child)
	{
		if (directory.empty() || directory[directory.size() - 1] == '/')
			return (directory + child);
		return (directory + "/" + child);
	}

	bool entryLess(const DirectoryEntry& left, const DirectoryEntry& right)
	{
		return (left.name < right.name);
	}

	bool shouldHideEntry(const std::string& name)
	{
		return (name.empty() || name[0] == '.');
	}

	std::string htmlEscape(const std::string& value)
	{
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] == '&')
				result += "&amp;";
			else if (value[i] == '<')
				result += "&lt;";
			else if (value[i] == '>')
				result += "&gt;";
			else if (value[i] == '"')
				result += "&quot;";
			else if (value[i] == '\'')
				result += "&#39;";
			else
				result += value[i];
		}
		return (result);
	}

	bool isUriSafeChar(unsigned char value)
	{
		if ((value >= 'A' && value <= 'Z')
			|| (value >= 'a' && value <= 'z')
			|| (value >= '0' && value <= '9'))
			return (true);
		return (value == '-' || value == '_' || value == '.' || value == '~');
	}

	std::string uriEncode(const std::string& value)
	{
		static const char* digits = "0123456789ABCDEF";
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
		{
			const unsigned char c = static_cast<unsigned char>(value[i]);

			if (isUriSafeChar(c))
				result += static_cast<char>(c);
			else
			{
				result += '%';
				result += digits[c >> 4];
				result += digits[c & 15];
			}
		}
		return (result);
	}

	std::string directoryUriBase(const std::string& uriPath)
	{
		if (!uriPath.empty() && uriPath[uriPath.size() - 1] == '/')
			return (uriPath);
		return (uriPath + "/");
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
		std::vector<std::string> indexes;

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
		indexes.push_back("index.html");
		return (handlePath(
				resolved.filesystemPath,
				indexes,
				false,
				resolved.uriPath));
	}

	HttpResponse StaticFileHandler::handlePath(
		const std::string& path,
		const std::vector<std::string>& indexes,
		bool autoindex,
		const std::string& uriPath) const
	{
		struct stat info;

		if (stat(path.c_str(), &info) != 0)
			return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
		if (isDirectoryMode(info.st_mode))
			return (buildDirectoryResponse(path, indexes, autoindex, uriPath));
		return (buildFileResponse(path));
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
			close(fd);
			return (ResponseBuilder::error(HTTP_STATUS_INTERNAL_SERVER_ERROR));
		}
		close(fd);
		return (ResponseBuilder::text(
				HTTP_STATUS_OK,
				body,
				mimeTypeForPath(path)));
	}

	HttpResponse StaticFileHandler::buildDirectoryResponse(
		const std::string& path,
		const std::vector<std::string>& indexes,
		bool autoindex,
		const std::string& uriPath) const
	{
		struct stat info;

		if (access(path.c_str(), R_OK | X_OK) != 0)
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (uriPath.empty() || uriPath[uriPath.size() - 1] != '/')
			return (ResponseBuilder::redirect(
					HTTP_STATUS_MOVED_PERMANENTLY,
					directoryUriBase(uriPath)));
		for (std::size_t i = 0; i < indexes.size(); ++i)
		{
			const std::string candidate = joinPath(path, indexes[i]);

			if (stat(candidate.c_str(), &info) == 0)
			{
				if (isRegularFileMode(info.st_mode))
					return (buildFileResponse(candidate));
				if (isDirectoryMode(info.st_mode))
					return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
			}
		}
		if (autoindex)
			return (buildAutoindexResponse(path, uriPath));
		return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
	}

	HttpResponse StaticFileHandler::buildAutoindexResponse(
		const std::string& path,
		const std::string& uriPath) const
	{
		DIR* directory;
		std::vector<DirectoryEntry> entries;
		struct dirent* entry;
		std::ostringstream body;
		const std::string baseUri = directoryUriBase(uriPath);

		directory = opendir(path.c_str());
		if (directory == 0)
		{
			if (errno == EACCES)
				return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
			return (ResponseBuilder::error(HTTP_STATUS_NOT_FOUND));
		}
		while ((entry = readdir(directory)) != 0)
		{
			const std::string name = entry->d_name;
			struct stat info;
			bool isDirectory;

			if (shouldHideEntry(name))
				continue;
			isDirectory = false;
			if (stat(joinPath(path, name).c_str(), &info) == 0)
				isDirectory = isDirectoryMode(info.st_mode);
			entries.push_back(DirectoryEntry(name, isDirectory));
		}
		closedir(directory);
		std::sort(entries.begin(), entries.end(), entryLess);
		body << "<!doctype html>\n"
			 << "<html lang=\"en\">\n"
			 << "<head><meta charset=\"utf-8\"><title>Index of "
			 << htmlEscape(baseUri) << "</title></head>\n"
			 << "<body>\n"
			 << "<h1>Index of " << htmlEscape(baseUri) << "</h1>\n"
			 << "<ul>\n";
		for (std::size_t i = 0; i < entries.size(); ++i)
		{
			const std::string displayName =
				entries[i].name + (entries[i].isDirectory ? "/" : "");
			const std::string href =
				baseUri + uriEncode(entries[i].name)
				+ (entries[i].isDirectory ? "/" : "");

			body << "<li><a href=\"" << htmlEscape(href) << "\">"
				 << htmlEscape(displayName) << "</a></li>\n";
		}
		body << "</ul>\n"
			 << "</body>\n"
			 << "</html>\n";
		return (ResponseBuilder::text(
				HTTP_STATUS_OK,
				body.str(),
				"text/html"));
	}
}
