#ifndef WEBSERV_ERROR_PAGE_HANDLER_HPP
# define WEBSERV_ERROR_PAGE_HANDLER_HPP

# include <string>

namespace webserv
{
	class ErrorPageHandler
	{
	public:
		static std::string	defaultErrorPage(int statusCode);

	private:
		ErrorPageHandler();
	};
}

#endif
