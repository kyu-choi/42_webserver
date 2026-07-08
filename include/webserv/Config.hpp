#ifndef WEBSERV_CONFIG_HPP
# define WEBSERV_CONFIG_HPP

# include "webserv/Http.hpp"
# include <map>
# include <string>
# include <vector>

namespace webserv
{
	enum DirectiveContext
	{
		DIRECTIVE_CONTEXT_TOP = 1,
		DIRECTIVE_CONTEXT_SERVER = 2,
		DIRECTIVE_CONTEXT_LOCATION = 4
	};

	struct DirectiveRule
	{
		std::string	name;
		unsigned	minArgs;
		unsigned	maxArgs;
		int			contexts;
		bool		duplicateAllowed;
		bool		inheritedByLocation;

		DirectiveRule(
			const std::string& nameValue,
			unsigned minArgsValue,
			unsigned maxArgsValue,
			int contextsValue,
			bool duplicateAllowedValue,
			bool inheritedByLocationValue);
	};

	struct ListenEndpoint
	{
		std::string		host;
		unsigned short	port;
	};

	struct LocationConfig
	{
		std::string							path;
		std::string							root;
		bool								rootSet;
		std::vector<std::string>			indexes;
		bool								indexesSet;
		std::vector<HttpMethod>				methods;
		bool								methodsSet;
		bool								autoindex;
		bool								autoindexSet;
		bool								uploadEnabled;
		bool								uploadSet;
		std::string							uploadStore;
		bool								uploadStoreSet;
		std::map<std::string, std::string>	cgiByExtension;
		int									redirectCode;
		std::string							redirectTarget;
		bool								redirectSet;
		std::size_t							clientMaxBodySize;
		bool								clientMaxBodySizeSet;
		std::map<int, std::string>			errorPages;
	};

	struct ServerConfig
	{
		std::vector<ListenEndpoint>			listen;
		std::string							root;
		std::vector<std::string>			indexes;
		std::size_t							clientMaxBodySize;
		std::map<int, std::string>			errorPages;
		std::vector<LocationConfig>			locations;
	};

	std::vector<DirectiveRule>	defaultDirectiveRules();
	ListenEndpoint				defaultListenEndpoint();
	LocationConfig				defaultLocationConfig();
	ServerConfig				defaultServerConfig();
	bool						directiveAllowedInContext(
									const DirectiveRule& rule,
									DirectiveContext context);
}

#endif
