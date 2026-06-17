#ifndef WEBSERV_PATH_POLICY_HPP
# define WEBSERV_PATH_POLICY_HPP

# include <string>

namespace webserv
{
	enum PathPolicyError
	{
		PATH_POLICY_OK,
		PATH_POLICY_BAD_URI,
		PATH_POLICY_BAD_ESCAPE,
		PATH_POLICY_TRAVERSAL,
		PATH_POLICY_LOCATION_MISMATCH
	};

	struct PathResolution
	{
		bool			ok;
		PathPolicyError	error;
		std::string		uriPath;
		std::string		relativePath;
		std::string		filesystemPath;
	};

	std::string		stripQueryString(const std::string& uri);
	PathResolution	resolveUriPath(
						const std::string& requestUri,
						const std::string& locationPrefix,
						const std::string& locationRoot);
	const char*		pathPolicyErrorName(PathPolicyError error);
}

#endif
