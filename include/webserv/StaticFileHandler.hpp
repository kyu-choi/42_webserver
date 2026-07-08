#ifndef WEBSERV_STATIC_FILE_HANDLER_HPP
# define WEBSERV_STATIC_FILE_HANDLER_HPP

# include "webserv/HttpRequest.hpp"
# include "webserv/HttpResponse.hpp"
# include <string>
# include <vector>

namespace webserv
{
	class StaticFileHandler
	{
	public:
		explicit StaticFileHandler(const std::string& root);

		HttpResponse	handleGet(const HttpRequest& request) const;
		HttpResponse	handlePath(
							const std::string& path,
							const std::vector<std::string>& indexes) const;

	private:
		std::string	_root;

		HttpResponse	buildFileResponse(const std::string& path) const;
		HttpResponse	buildDirectoryResponse(
							const std::string& path,
							const std::vector<std::string>& indexes) const;
	};
}

#endif
