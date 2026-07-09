#ifndef WEBSERV_DELETE_HANDLER_HPP
# define WEBSERV_DELETE_HANDLER_HPP

# include "webserv/HttpResponse.hpp"
# include <string>

namespace webserv
{
	class DeleteHandler
	{
	public:
		DeleteHandler();

		HttpResponse	handleDelete(
							const std::string& filesystemPath,
							const std::string& relativePath) const;
	};
}

#endif
