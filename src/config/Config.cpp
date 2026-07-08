#include "webserv/Config.hpp"

namespace webserv
{
	DirectiveRule::DirectiveRule(
		const std::string& nameValue,
		unsigned minArgsValue,
		unsigned maxArgsValue,
		int contextsValue,
		bool duplicateAllowedValue,
		bool inheritedByLocationValue)
		: name(nameValue),
		  minArgs(minArgsValue),
		  maxArgs(maxArgsValue),
		  contexts(contextsValue),
		  duplicateAllowed(duplicateAllowedValue),
		  inheritedByLocation(inheritedByLocationValue)
	{
	}

	std::vector<DirectiveRule> defaultDirectiveRules()
	{
		std::vector<DirectiveRule> rules;

		rules.push_back(DirectiveRule("server", 0, 0,
				DIRECTIVE_CONTEXT_TOP, true, false));
		rules.push_back(DirectiveRule("listen", 1, 1,
				DIRECTIVE_CONTEXT_SERVER, true, false));
		rules.push_back(DirectiveRule("root", 1, 1,
				DIRECTIVE_CONTEXT_SERVER | DIRECTIVE_CONTEXT_LOCATION,
				false, true));
		rules.push_back(DirectiveRule("index", 1, 16,
				DIRECTIVE_CONTEXT_SERVER | DIRECTIVE_CONTEXT_LOCATION,
				false, true));
		rules.push_back(DirectiveRule("client_max_body_size", 1, 1,
				DIRECTIVE_CONTEXT_SERVER | DIRECTIVE_CONTEXT_LOCATION,
				false, true));
		rules.push_back(DirectiveRule("error_page", 2, 2,
				DIRECTIVE_CONTEXT_SERVER | DIRECTIVE_CONTEXT_LOCATION,
				true, true));
		rules.push_back(DirectiveRule("location", 1, 1,
				DIRECTIVE_CONTEXT_SERVER, true, false));
		rules.push_back(DirectiveRule("methods", 1, 3,
				DIRECTIVE_CONTEXT_LOCATION, false, false));
		rules.push_back(DirectiveRule("return", 2, 2,
				DIRECTIVE_CONTEXT_LOCATION, false, false));
		rules.push_back(DirectiveRule("autoindex", 1, 1,
				DIRECTIVE_CONTEXT_LOCATION, false, false));
		rules.push_back(DirectiveRule("upload", 1, 1,
				DIRECTIVE_CONTEXT_LOCATION, false, false));
		rules.push_back(DirectiveRule("upload_store", 1, 1,
				DIRECTIVE_CONTEXT_LOCATION, false, false));
		rules.push_back(DirectiveRule("cgi", 2, 2,
				DIRECTIVE_CONTEXT_LOCATION, true, false));
		return (rules);
	}

	ListenEndpoint defaultListenEndpoint()
	{
		ListenEndpoint endpoint;

		endpoint.host = "127.0.0.1";
		endpoint.port = 8080;
		return (endpoint);
	}

	LocationConfig defaultLocationConfig()
	{
		LocationConfig location;

		location.path = "/";
		location.root = "";
		location.rootSet = false;
		location.indexes.push_back("index.html");
		location.indexesSet = false;
		location.methods.push_back(HTTP_METHOD_GET);
		location.methodsSet = false;
		location.autoindex = false;
		location.autoindexSet = false;
		location.uploadEnabled = false;
		location.uploadSet = false;
		location.uploadStore = "";
		location.uploadStoreSet = false;
		location.redirectCode = 0;
		location.redirectTarget = "";
		location.redirectSet = false;
		location.clientMaxBodySize = 0;
		location.clientMaxBodySizeSet = false;
		return (location);
	}

	ServerConfig defaultServerConfig()
	{
		ServerConfig server;

		server.listen.push_back(defaultListenEndpoint());
		server.root = "./www";
		server.indexes.push_back("index.html");
		server.clientMaxBodySize = 10 * 1024 * 1024;
		return (server);
	}

	bool directiveAllowedInContext(
		const DirectiveRule& rule,
		DirectiveContext context)
	{
		return ((rule.contexts & context) != 0);
	}
}
