#include "webserv/PathPolicy.hpp"
#include <cctype>
#include <vector>

namespace
{
	bool isHex(char value)
	{
		return (std::isxdigit(static_cast<unsigned char>(value)) != 0);
	}

	int hexValue(char value)
	{
		if (value >= '0' && value <= '9')
			return (value - '0');
		if (value >= 'a' && value <= 'f')
			return (value - 'a' + 10);
		if (value >= 'A' && value <= 'F')
			return (value - 'A' + 10);
		return (0);
	}

	bool percentDecode(const std::string& input, std::string& output)
	{
		std::size_t index;

		output.clear();
		index = 0;
		while (index < input.size())
		{
			if (input[index] == '%')
			{
				if (index + 2 >= input.size()
					|| !isHex(input[index + 1])
					|| !isHex(input[index + 2]))
					return (false);
				const char decoded = static_cast<char>(
					hexValue(input[index + 1]) * 16 + hexValue(input[index + 2]));
				if (decoded == '\0')
					return (false);
				output += decoded;
				index += 3;
			}
			else
			{
				output += input[index];
				++index;
			}
		}
		return (true);
	}

	std::vector<std::string> splitSlash(const std::string& path)
	{
		std::vector<std::string> segments;
		std::size_t start;
		std::size_t end;

		start = 0;
		while (start < path.size())
		{
			while (start < path.size() && path[start] == '/')
				++start;
			end = start;
			while (end < path.size() && path[end] != '/')
				++end;
			if (start < end)
				segments.push_back(path.substr(start, end - start));
			start = end;
		}
		return (segments);
	}

	webserv::PathResolution makeError(webserv::PathPolicyError error)
	{
		webserv::PathResolution result;

		result.ok = false;
		result.error = error;
		return (result);
	}

	std::string joinPath(const std::string& root, const std::string& relative)
	{
		std::string cleanedRoot;

		cleanedRoot = root;
		while (cleanedRoot.size() > 1
			&& cleanedRoot[cleanedRoot.size() - 1] == '/')
			cleanedRoot.erase(cleanedRoot.size() - 1);
		if (relative.empty())
			return (cleanedRoot);
		if (cleanedRoot.empty() || cleanedRoot == "/")
			return ("/" + relative);
		return (cleanedRoot + "/" + relative);
	}

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
}

namespace webserv
{
	std::string stripQueryString(const std::string& uri)
	{
		std::size_t query;
		std::size_t fragment;
		std::size_t end;

		query = uri.find('?');
		fragment = uri.find('#');
		end = uri.size();
		if (query != std::string::npos && query < end)
			end = query;
		if (fragment != std::string::npos && fragment < end)
			end = fragment;
		return (uri.substr(0, end));
	}

	PathResolution resolveUriPath(
		const std::string& requestUri,
		const std::string& locationPrefix,
		const std::string& locationRoot)
	{
		std::string uriOnly;
		std::string decoded;
		std::vector<std::string> segments;
		std::string normalized;
		std::string relative;
		std::size_t index;
		PathResolution result;

		uriOnly = stripQueryString(requestUri);
		if (uriOnly.empty() || uriOnly[0] != '/')
			return (makeError(PATH_POLICY_BAD_URI));
		if (!percentDecode(uriOnly, decoded))
			return (makeError(PATH_POLICY_BAD_ESCAPE));
		segments = splitSlash(decoded);
		normalized = "/";
		relative = "";
		for (index = 0; index < segments.size(); ++index)
		{
			if (segments[index] == "..")
				return (makeError(PATH_POLICY_TRAVERSAL));
			if (segments[index] == ".")
				continue;
			if (normalized.size() > 1)
				normalized += "/";
			normalized += segments[index];
		}
		if (!locationMatches(normalized, locationPrefix))
			return (makeError(PATH_POLICY_LOCATION_MISMATCH));
		if (locationPrefix == "/")
			relative = normalized.substr(1);
		else if (normalized.size() > locationPrefix.size())
			relative = normalized.substr(locationPrefix.size() + 1);
		result.ok = true;
		result.error = PATH_POLICY_OK;
		result.uriPath = normalized;
		result.relativePath = relative;
		result.filesystemPath = joinPath(locationRoot, relative);
		return (result);
	}

	const char* pathPolicyErrorName(PathPolicyError error)
	{
		switch (error)
		{
			case PATH_POLICY_OK:
				return ("OK");
			case PATH_POLICY_BAD_URI:
				return ("BAD_URI");
			case PATH_POLICY_BAD_ESCAPE:
				return ("BAD_ESCAPE");
			case PATH_POLICY_TRAVERSAL:
				return ("TRAVERSAL");
			case PATH_POLICY_LOCATION_MISMATCH:
				return ("LOCATION_MISMATCH");
		}
		return ("BAD_URI");
	}
}
