#include "webserv/UploadHandler.hpp"
#include "webserv/ResponseBuilder.hpp"
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	bool isDirectoryMode(mode_t mode)
	{
		return (S_ISDIR(mode));
	}

	std::string lowerString(const std::string& value)
	{
		std::string result;

		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] >= 'A' && value[i] <= 'Z')
				result += static_cast<char>(value[i] - 'A' + 'a');
			else
				result += value[i];
		}
		return (result);
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin;
		std::size_t end;

		begin = 0;
		while (begin < value.size()
			&& (value[begin] == ' ' || value[begin] == '\t'))
			++begin;
		end = value.size();
		while (end > begin
			&& (value[end - 1] == ' ' || value[end - 1] == '\t'))
			--end;
		return (value.substr(begin, end - begin));
	}

	std::string joinPath(const std::string& directory, const std::string& child)
	{
		if (directory.empty() || directory[directory.size() - 1] == '/')
			return (directory + child);
		return (directory + "/" + child);
	}

	bool isValidFilename(const std::string& filename)
	{
		if (filename.empty() || filename == "." || filename == "..")
			return (false);
		for (std::size_t i = 0; i < filename.size(); ++i)
		{
			const unsigned char c = static_cast<unsigned char>(filename[i]);

			if (filename[i] == '/' || filename[i] == '\\')
				return (false);
			if (c < 32 || c == 127)
				return (false);
		}
		return (true);
	}

	std::string stripQuotes(const std::string& value)
	{
		if (value.size() >= 2
			&& value[0] == '"'
			&& value[value.size() - 1] == '"')
			return (value.substr(1, value.size() - 2));
		return (value);
	}

	std::string headerParameter(
		const std::string& headerValue,
		const std::string& parameterName)
	{
		std::size_t start;

		start = 0;
		while (start < headerValue.size())
		{
			std::size_t end = headerValue.find(';', start);
			std::string token;
			std::size_t equal;

			if (end == std::string::npos)
				end = headerValue.size();
			token = trim(headerValue.substr(start, end - start));
			equal = token.find('=');
			if (equal != std::string::npos)
			{
				const std::string name = lowerString(trim(token.substr(0, equal)));
				const std::string value = trim(token.substr(equal + 1));

				if (name == parameterName)
					return (stripQuotes(value));
			}
			start = end + 1;
		}
		return ("");
	}

	std::string multipartBoundary(const webserv::HttpRequest& request)
	{
		const std::string contentType = request.header("Content-Type");
		const std::string lowered = lowerString(contentType);

		if (lowered.find("multipart/form-data") == std::string::npos)
			return ("");
		return (headerParameter(contentType, "boundary"));
	}

	std::string contentDispositionHeader(const std::string& headerBlock)
	{
		std::size_t start;

		start = 0;
		while (start < headerBlock.size())
		{
			std::size_t end = headerBlock.find("\r\n", start);
			std::string line;
			std::size_t colon;

			if (end == std::string::npos)
				end = headerBlock.size();
			line = headerBlock.substr(start, end - start);
			colon = line.find(':');
			if (colon != std::string::npos)
			{
				const std::string name = lowerString(trim(line.substr(0, colon)));

				if (name == "content-disposition")
					return (trim(line.substr(colon + 1)));
			}
			start = end + 2;
		}
		return ("");
	}

	std::string generatedRawFilename()
	{
		static unsigned long counter = 0;
		std::ostringstream stream;

		++counter;
		stream << "upload-" << static_cast<long>(std::time(0))
			   << "-" << counter << ".bin";
		return (stream.str());
	}

	bool startsWithAt(
		const std::string& value,
		std::size_t position,
		const std::string& prefix)
	{
		if (position > value.size())
			return (false);
		if (value.size() - position < prefix.size())
			return (false);
		return (value.compare(position, prefix.size(), prefix) == 0);
	}

	bool writeAll(int fd, const std::string& content)
	{
		std::size_t written;

		written = 0;
		while (written < content.size())
		{
			const ssize_t result = write(
				fd,
				content.data() + written,
				content.size() - written);

			if (result > 0)
			{
				written += static_cast<std::size_t>(result);
				continue;
			}
			if (result < 0 && errno == EINTR)
				continue;
			return (false);
		}
		return (true);
	}
}

namespace webserv
{
	UploadHandler::SavedFile::SavedFile(
		const std::string& filenameValue,
		bool createdValue)
		: filename(filenameValue), created(createdValue)
	{
	}

	UploadHandler::UploadHandler(const std::string& uploadStore)
		: _uploadStore(uploadStore)
	{
	}

	HttpResponse UploadHandler::handlePost(const HttpRequest& request) const
	{
		const std::string boundary = multipartBoundary(request);
		const std::string contentType = request.header("Content-Type");

		if (!ensureUploadStore())
			return (ResponseBuilder::error(HTTP_STATUS_FORBIDDEN));
		if (lowerString(contentType).find("multipart/form-data")
			!= std::string::npos)
		{
			if (boundary.empty())
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			return (handleMultipart(request, boundary));
		}
		return (handleRawBody(request));
	}

	HttpResponse UploadHandler::handleRawBody(const HttpRequest& request) const
	{
		std::vector<SavedFile> savedFiles;
		bool created;
		const std::string filename = generatedRawFilename();

		if (!saveFile(filename, request.body(), created))
			return (ResponseBuilder::error(HTTP_STATUS_INTERNAL_SERVER_ERROR));
		savedFiles.push_back(SavedFile(filename, created));
		return (buildSuccessResponse(savedFiles));
	}

	HttpResponse UploadHandler::handleMultipart(
		const HttpRequest& request,
		const std::string& boundary) const
	{
		const std::string& body = request.body();
		const std::string marker = "--" + boundary;
		const std::string prefixedMarker = "\r\n" + marker;
		std::vector<SavedFile> savedFiles;
		std::size_t position;

		if (!startsWithAt(body, 0, marker))
			return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
		position = marker.size();
		while (true)
		{
			if (startsWithAt(body, position, "--"))
				break;
			if (!startsWithAt(body, position, "\r\n"))
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			position += 2;
			const std::size_t headerEnd = body.find("\r\n\r\n", position);

			if (headerEnd == std::string::npos)
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			const std::string headerBlock =
				body.substr(position, headerEnd - position);
			const std::string disposition =
				contentDispositionHeader(headerBlock);
			const std::string filename =
				headerParameter(disposition, "filename");
			const std::size_t contentStart = headerEnd + 4;
			const std::size_t nextBoundary =
				body.find(prefixedMarker, contentStart);

			if (nextBoundary == std::string::npos)
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			if (!filename.empty())
			{
				bool created;
				const std::string content =
					body.substr(contentStart, nextBoundary - contentStart);

				if (!isValidFilename(filename))
					return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
				if (!saveFile(filename, content, created))
					return (ResponseBuilder::error(
							HTTP_STATUS_INTERNAL_SERVER_ERROR));
				savedFiles.push_back(SavedFile(filename, created));
			}
			else if (disposition.find("filename=") != std::string::npos)
				return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
			position = nextBoundary + prefixedMarker.size();
		}
		if (savedFiles.empty())
			return (ResponseBuilder::error(HTTP_STATUS_BAD_REQUEST));
		return (buildSuccessResponse(savedFiles));
	}

	HttpResponse UploadHandler::buildSuccessResponse(
		const std::vector<SavedFile>& savedFiles) const
	{
		bool anyCreated;
		std::string body;
		HttpResponse response;

		anyCreated = false;
		for (std::size_t i = 0; i < savedFiles.size(); ++i)
		{
			if (savedFiles[i].created)
				anyCreated = true;
			body += savedFiles[i].created ? "created " : "updated ";
			body += savedFiles[i].filename;
			body += "\n";
		}
		response = ResponseBuilder::text(
			anyCreated ? HTTP_STATUS_CREATED : HTTP_STATUS_OK,
			body,
			"text/plain");
		if (!savedFiles.empty())
			response.setHeader("Location", "/uploads/" + savedFiles[0].filename);
		return (response);
	}

	bool UploadHandler::ensureUploadStore() const
	{
		struct stat info;

		if (stat(_uploadStore.c_str(), &info) != 0)
			return (false);
		if (!isDirectoryMode(info.st_mode))
			return (false);
		return (access(_uploadStore.c_str(), W_OK | X_OK) == 0);
	}

	bool UploadHandler::saveFile(
		const std::string& filename,
		const std::string& content,
		bool& created) const
	{
		const std::string path = joinPath(_uploadStore, filename);
		const bool existed = (access(path.c_str(), F_OK) == 0);
		const int fd = open(
			path.c_str(),
			O_WRONLY | O_CREAT | O_TRUNC,
			0644);

		if (fd < 0)
			return (false);
		if (!writeAll(fd, content))
		{
			close(fd);
			return (false);
		}
		if (close(fd) != 0)
			return (false);
		created = !existed;
		return (true);
	}
}
