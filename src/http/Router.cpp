#include "webserv/Router.hpp"
#include "webserv/PathPolicy.hpp"

namespace
{
	bool locationMatches(const std::string& path, const std::string& location)
	{
		if (location == "/")
			return (!path.empty() && path[0] == '/');
		if (path == location)
			return (true);
		if (path.size() <= location.size())
			return (false);
		return (path.compare(0, location.size(), location) == 0
			&& path[location.size()] == '/');
	}

	int statusForPathError(webserv::PathPolicyError error)
	{
		if (error == webserv::PATH_POLICY_BAD_URI
			|| error == webserv::PATH_POLICY_BAD_ESCAPE)
			return (webserv::HTTP_STATUS_BAD_REQUEST);
		if (error == webserv::PATH_POLICY_TRAVERSAL)
			return (webserv::HTTP_STATUS_FORBIDDEN);
		return (webserv::HTTP_STATUS_NOT_FOUND);
	}

	webserv::RouteResult makeRouteError(int statusCode)
	{
		webserv::RouteResult result;

		result.ok = false;
		result.statusCode = statusCode;
		return (result);
	}

	const webserv::LocationConfig* selectLocation(
		const webserv::ServerConfig& server,
		const std::string& uriPath)
	{
		const webserv::LocationConfig* selected;
		std::size_t selectedLength;

		selected = 0;
		selectedLength = 0;
		for (std::size_t i = 0; i < server.locations.size(); ++i)
		{
			const webserv::LocationConfig& location = server.locations[i];

			if (locationMatches(uriPath, location.path)
				&& location.path.size() >= selectedLength)
			{
				selected = &location;
				selectedLength = location.path.size();
			}
		}
		return (selected);
	}

	void mergeErrorPages(
		std::map<int, std::string>& target,
		const std::map<int, std::string>& source)
	{
		for (std::map<int, std::string>::const_iterator it = source.begin();
			it != source.end(); ++it)
			target[it->first] = it->second;
	}

	webserv::EffectiveRouteConfig buildEffectiveConfig(
		const webserv::ServerConfig& server,
		const webserv::LocationConfig* location)
	{
		webserv::EffectiveRouteConfig effective;

		effective.root = server.root;
		effective.indexes = server.indexes;
		effective.methods.push_back(webserv::HTTP_METHOD_GET);
		effective.clientMaxBodySize = server.clientMaxBodySize;
		effective.errorPages = server.errorPages;
		if (location == 0)
			return (effective);
		if (location->rootSet)
			effective.root = location->root;
		if (location->indexesSet)
			effective.indexes = location->indexes;
		if (location->methodsSet)
			effective.methods = location->methods;
		if (location->autoindexSet)
			effective.autoindex = location->autoindex;
		if (location->uploadSet)
			effective.uploadEnabled = location->uploadEnabled;
		if (location->uploadStoreSet)
			effective.uploadStore = location->uploadStore;
		if (location->redirectSet)
		{
			effective.redirectCode = location->redirectCode;
			effective.redirectTarget = location->redirectTarget;
		}
		if (location->clientMaxBodySizeSet)
			effective.clientMaxBodySize = location->clientMaxBodySize;
		mergeErrorPages(effective.errorPages, location->errorPages);
		effective.cgiByExtension = location->cgiByExtension;
		return (effective);
	}
}

namespace webserv
{
	EffectiveRouteConfig::EffectiveRouteConfig()
		: root(),
		  indexes(),
		  methods(),
		  autoindex(false),
		  uploadEnabled(false),
		  uploadStore(),
		  cgiByExtension(),
		  redirectCode(0),
		  redirectTarget(),
		  clientMaxBodySize(0),
		  errorPages()
	{
	}

	RouteResult::RouteResult()
		: ok(false),
		  statusCode(0),
		  uriPath(),
		  queryString(),
		  locationPrefix(),
		  relativePath(),
		  filesystemPath(),
		  effective()
	{
	}

	RouteResult Router::route(
		const HttpRequest& request,
		const ServerConfig& server)
	{
		PathResolution uriResolution;
		PathResolution filesystemResolution;
		const LocationConfig* location;
		std::string locationPrefix;
		RouteResult result;

		uriResolution = resolveUriPath(request.rawTarget(), "/", "");
		if (!uriResolution.ok)
			return (makeRouteError(statusForPathError(uriResolution.error)));
		location = selectLocation(server, uriResolution.uriPath);
		if (location == 0)
			locationPrefix = "/";
		else
			locationPrefix = location->path;
		result.effective = buildEffectiveConfig(server, location);
		filesystemResolution = resolveUriPath(
				request.rawTarget(),
				locationPrefix,
				result.effective.root);
		if (!filesystemResolution.ok)
			return (makeRouteError(
					statusForPathError(filesystemResolution.error)));
		result.ok = true;
		result.statusCode = HTTP_STATUS_OK;
		result.uriPath = filesystemResolution.uriPath;
		result.queryString = request.query();
		result.locationPrefix = locationPrefix;
		result.relativePath = filesystemResolution.relativePath;
		result.filesystemPath = filesystemResolution.filesystemPath;
		return (result);
	}
}
