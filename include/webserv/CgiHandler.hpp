#ifndef WEBSERV_CGI_HANDLER_HPP
# define WEBSERV_CGI_HANDLER_HPP

# include "webserv/HttpRequest.hpp"
# include "webserv/HttpResponse.hpp"
# include "webserv/Router.hpp"
# include <string>
# include <sys/types.h>

namespace webserv
{
	struct CgiExecution
	{
		bool		ok;
		int			statusCode;
		pid_t		pid;
		int			stdinFd;
		int			stdoutFd;
		std::string	requestBody;

		CgiExecution();
	};

	class CgiHandler
	{
	public:
		static bool			isCgiRequest(const RouteResult& route);
		static CgiExecution	start(
								const HttpRequest& request,
								const RouteResult& route);
		static bool			buildResponse(
								const std::string& output,
								HttpResponse& response);

	private:
		CgiHandler();
	};
}

#endif
