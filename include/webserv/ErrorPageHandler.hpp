#ifndef WEBSERV_ERROR_PAGE_HANDLER_HPP
# define WEBSERV_ERROR_PAGE_HANDLER_HPP

# include <map>
# include <string>

namespace webserv
{
	class ErrorPageHandler
	{
	public:
		static std::string	defaultErrorPage(int statusCode);
		static bool			loadCustomErrorPage(
								int statusCode,
								const std::map<int, std::string>& errorPages,
								const std::string& root,
								std::string& body,
								std::string& contentType);

	private:
		ErrorPageHandler();
	};
}

#endif
