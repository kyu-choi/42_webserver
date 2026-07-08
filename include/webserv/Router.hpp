#ifndef WEBSERV_ROUTER_HPP
# define WEBSERV_ROUTER_HPP

# include "webserv/Config.hpp"
# include "webserv/HttpRequest.hpp"
# include <map>
# include <string>
# include <vector>

namespace webserv
{
	struct EffectiveRouteConfig
	{
		std::string							root;
		std::vector<std::string>			indexes;
		std::vector<HttpMethod>				methods;
		bool								autoindex;
		bool								uploadEnabled;
		std::string							uploadStore;
		std::map<std::string, std::string>	cgiByExtension;
		int									redirectCode;
		std::string							redirectTarget;
		std::size_t							clientMaxBodySize;
		std::map<int, std::string>			errorPages;

		EffectiveRouteConfig();
	};

	struct RouteResult
	{
		bool					ok;
		int						statusCode;
		std::string				uriPath;
		std::string				queryString;
		std::string				locationPrefix;
		std::string				relativePath;
		std::string				filesystemPath;
		EffectiveRouteConfig	effective;

		RouteResult();
	};

	class Router
	{
	public:
		static RouteResult	route(
								const HttpRequest& request,
								const ServerConfig& server);

	private:
		Router();
	};
}

#endif
