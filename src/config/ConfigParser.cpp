#include "webserv/ConfigParser.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace
{
	bool vectorContains(
		const std::vector<std::string>& values,
		const std::string& value)
	{
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (values[i] == value)
				return (true);
		}
		return (false);
	}

	bool isNumber(const std::string& value)
	{
		if (value.empty())
			return (false);
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] < '0' || value[i] > '9')
				return (false);
		}
		return (true);
	}

	unsigned long parseUnsignedLong(const std::string& value, bool& ok)
	{
		char* end;
		unsigned long parsed;

		errno = 0;
		end = 0;
		parsed = std::strtoul(value.c_str(), &end, 10);
		ok = (errno == 0 && end != 0 && *end == '\0');
		return (parsed);
	}

	bool isValidIpv4Host(const std::string& host)
	{
		std::size_t start;
		unsigned parts;

		start = 0;
		parts = 0;
		while (start <= host.size())
		{
			const std::size_t dot = host.find('.', start);
			const std::string part = host.substr(start,
				(dot == std::string::npos) ? std::string::npos : dot - start);
			bool ok;
			unsigned long value;

			if (part.empty() || !isNumber(part))
				return (false);
			value = parseUnsignedLong(part, ok);
			if (!ok || value > 255)
				return (false);
			++parts;
			if (dot == std::string::npos)
				break;
			start = dot + 1;
		}
		return (parts == 4);
	}

	bool isDirectiveKeyword(const std::string& value)
	{
		static const char* keywords[] = {
			"server",
			"listen",
			"root",
			"index",
			"client_max_body_size",
			"error_page",
			"location",
			"methods",
			"return",
			"autoindex",
			"upload",
			"upload_store",
			"cgi",
			0
		};

		for (std::size_t i = 0; keywords[i] != 0; ++i)
		{
			if (value == keywords[i])
				return (true);
		}
		return (false);
	}

	std::string linePrefix(std::size_t line)
	{
		std::ostringstream stream;

		stream << "config line " << line << ": ";
		return (stream.str());
	}
}

namespace webserv
{
	ConfigParser::Token::Token(
		const std::string& valueValue,
		std::size_t lineValue)
		: value(valueValue), line(lineValue)
	{
	}

	ConfigParser::ConfigParser()
		: _tokens(), _position(0)
	{
	}

	std::vector<ServerConfig> ConfigParser::parseFile(const std::string& path)
	{
		_tokens = tokenize(readFile(path));
		_position = 0;
		return (parseServers());
	}

	std::string ConfigParser::readFile(const std::string& path) const
	{
		const int fd = open(path.c_str(), O_RDONLY);
		char buffer[4096];
		std::string content;

		if (fd < 0)
			throw std::runtime_error("config: cannot open " + path);
		while (true)
		{
			const ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

			if (bytesRead > 0)
			{
				content.append(buffer, static_cast<std::size_t>(bytesRead));
				continue;
			}
			if (bytesRead == 0)
				break;
			close(fd);
			throw std::runtime_error("config: cannot read " + path);
		}
		close(fd);
		return (content);
	}

	std::vector<ConfigParser::Token> ConfigParser::tokenize(
		const std::string& content) const
	{
		std::vector<Token> tokens;
		std::string current;
		std::size_t line;

		line = 1;
		for (std::size_t i = 0; i < content.size(); ++i)
		{
			const char c = content[i];

			if (c == '#')
			{
				if (!current.empty())
				{
					tokens.push_back(Token(current, line));
					current.clear();
				}
				while (i < content.size() && content[i] != '\n')
					++i;
				if (i < content.size() && content[i] == '\n')
					++line;
				continue;
			}
			if (c == '\n')
			{
				if (!current.empty())
				{
					tokens.push_back(Token(current, line));
					current.clear();
				}
				++line;
				continue;
			}
			if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f')
			{
				if (!current.empty())
				{
					tokens.push_back(Token(current, line));
					current.clear();
				}
				continue;
			}
			if (c == '{' || c == '}' || c == ';')
			{
				if (!current.empty())
				{
					tokens.push_back(Token(current, line));
					current.clear();
				}
				tokens.push_back(Token(std::string(1, c), line));
				continue;
			}
			current += c;
		}
		if (!current.empty())
			tokens.push_back(Token(current, line));
		return (tokens);
	}

	std::vector<ServerConfig> ConfigParser::parseServers()
	{
		std::vector<ServerConfig> servers;

		while (!atEnd())
		{
			if (peek().value != "server")
				fail("expected server block");
			servers.push_back(parseServerBlock());
		}
		if (servers.empty())
			throw std::runtime_error("config: no server block found");
		return (servers);
	}

	ServerConfig ConfigParser::parseServerBlock()
	{
		ServerConfig server = defaultServerConfig();
		bool listenSet = false;
		std::vector<std::string> seen;

		expect("server");
		expect("{");
		while (!atEnd() && peek().value != "}")
		{
			const std::string name = consume().value;

			if (name == "location")
			{
				server.locations.push_back(parseLocationBlock());
				continue;
			}
			applyServerDirective(
				server,
				name,
				parseDirectiveArguments(),
				listenSet,
				seen);
		}
		expect("}");
		return (server);
	}

	LocationConfig ConfigParser::parseLocationBlock()
	{
		LocationConfig location = defaultLocationConfig();
		std::vector<std::string> seen;

		if (atEnd())
			fail("expected location prefix");
		location.path = consume().value;
		if (location.path.empty() || location.path[0] != '/')
			fail("location prefix must start with /");
		expect("{");
		while (!atEnd() && peek().value != "}")
		{
			const std::string name = consume().value;

			if (name == "location")
				fail("nested location is not allowed");
			applyLocationDirective(
				location,
				name,
				parseDirectiveArguments(),
				seen);
		}
		expect("}");
		validateLocation(location);
		return (location);
	}

	std::vector<std::string> ConfigParser::parseDirectiveArguments()
	{
		std::vector<std::string> args;
		std::size_t previousLine;

		previousLine = 0;
		while (!atEnd() && peek().value != ";")
		{
			if (peek().value == "{" || peek().value == "}")
				fail("missing semicolon");
			if (!args.empty()
				&& peek().line > previousLine
				&& isDirectiveKeyword(peek().value))
				fail("missing semicolon before " + peek().value);
			previousLine = peek().line;
			args.push_back(consume().value);
		}
		expect(";");
		return (args);
	}

	bool ConfigParser::atEnd() const
	{
		return (_position >= _tokens.size());
	}

	const ConfigParser::Token& ConfigParser::peek() const
	{
		if (atEnd())
			throw std::runtime_error("config: unexpected end of file");
		return (_tokens[_position]);
	}

	const ConfigParser::Token& ConfigParser::consume()
	{
		const Token& token = peek();

		++_position;
		return (token);
	}

	void ConfigParser::expect(const std::string& value)
	{
		if (atEnd())
			throw std::runtime_error("config: expected " + value
				+ ", got end of file");
		if (peek().value != value)
			fail("expected " + value + ", got " + peek().value);
		consume();
	}

	void ConfigParser::fail(const std::string& message) const
	{
		if (atEnd())
			throw std::runtime_error("config: " + message);
		throw std::runtime_error(linePrefix(peek().line) + message);
	}

	void ConfigParser::applyServerDirective(
		ServerConfig& server,
		const std::string& name,
		const std::vector<std::string>& args,
		bool& listenSet,
		std::vector<std::string>& seen)
	{
		if (name == "listen")
		{
			if (args.size() != 1)
				fail("listen expects 1 argument");
			if (!listenSet)
			{
				server.listen.clear();
				listenSet = true;
			}
			server.listen.push_back(parseListen(args[0]));
		}
		else if (name == "root")
		{
			if (vectorContains(seen, name))
				fail("duplicate root directive");
			if (args.size() != 1)
				fail("root expects 1 argument");
			server.root = args[0];
			seen.push_back(name);
		}
		else if (name == "index")
		{
			if (vectorContains(seen, name))
				fail("duplicate index directive");
			if (args.empty() || args.size() > 16)
				fail("index expects 1 to 16 arguments");
			server.indexes = args;
			seen.push_back(name);
		}
		else if (name == "client_max_body_size")
		{
			if (vectorContains(seen, name))
				fail("duplicate client_max_body_size directive");
			if (args.size() != 1)
				fail("client_max_body_size expects 1 argument");
			server.clientMaxBodySize = parseBodySize(args[0]);
			seen.push_back(name);
		}
		else if (name == "error_page")
		{
			if (args.size() != 2)
				fail("error_page expects 2 arguments");
			server.errorPages[parseStatusCode(args[0])] = args[1];
		}
		else
			fail("unknown or invalid server directive: " + name);
	}

	void ConfigParser::applyLocationDirective(
		LocationConfig& location,
		const std::string& name,
		const std::vector<std::string>& args,
		std::vector<std::string>& seen)
	{
		if (name != "error_page" && name != "cgi"
			&& vectorContains(seen, name))
			fail("duplicate " + name + " directive");
		if (name == "root")
		{
			if (args.size() != 1)
				fail("root expects 1 argument");
			location.root = args[0];
			location.rootSet = true;
		}
		else if (name == "index")
		{
			if (args.empty() || args.size() > 16)
				fail("index expects 1 to 16 arguments");
			location.indexes = args;
			location.indexesSet = true;
		}
		else if (name == "client_max_body_size")
		{
			if (args.size() != 1)
				fail("client_max_body_size expects 1 argument");
			location.clientMaxBodySize = parseBodySize(args[0]);
			location.clientMaxBodySizeSet = true;
		}
		else if (name == "error_page")
		{
			if (args.size() != 2)
				fail("error_page expects 2 arguments");
			location.errorPages[parseStatusCode(args[0])] = args[1];
		}
		else if (name == "methods")
		{
			if (args.empty() || args.size() > 3)
				fail("methods expects 1 to 3 arguments");
			location.methods.clear();
			for (std::size_t i = 0; i < args.size(); ++i)
				location.methods.push_back(parseAllowedMethod(args[i]));
			location.methodsSet = true;
		}
		else if (name == "return")
		{
			if (args.size() != 2)
				fail("return expects 2 arguments");
			location.redirectCode = parseStatusCode(args[0]);
			if (location.redirectCode != HTTP_STATUS_MOVED_PERMANENTLY
				&& location.redirectCode != HTTP_STATUS_FOUND)
				fail("return only supports 301 or 302");
			location.redirectTarget = args[1];
			location.redirectSet = true;
		}
		else if (name == "autoindex")
		{
			if (args.size() != 1)
				fail("autoindex expects 1 argument");
			location.autoindex = parseOnOff(args[0]);
			location.autoindexSet = true;
		}
		else if (name == "upload")
		{
			if (args.size() != 1)
				fail("upload expects 1 argument");
			location.uploadEnabled = parseOnOff(args[0]);
			location.uploadSet = true;
		}
		else if (name == "upload_store")
		{
			if (args.size() != 1)
				fail("upload_store expects 1 argument");
			location.uploadStore = args[0];
			location.uploadStoreSet = true;
		}
		else if (name == "cgi")
		{
			if (args.size() != 2)
				fail("cgi expects 2 arguments");
			if (args[0].empty() || args[0][0] != '.')
				fail("cgi extension must start with .");
			location.cgiByExtension[args[0]] = args[1];
		}
		else
			fail("unknown or invalid location directive: " + name);
		if (name != "error_page" && name != "cgi")
			seen.push_back(name);
	}

	void ConfigParser::validateLocation(const LocationConfig& location) const
	{
		if (location.uploadEnabled && !location.uploadStoreSet)
			fail("upload on requires upload_store");
	}

	ListenEndpoint ConfigParser::parseListen(const std::string& value) const
	{
		ListenEndpoint endpoint;
		std::string host;
		std::string portString;
		bool ok;
		unsigned long port;
		const std::size_t colon = value.find(':');

		if (colon == std::string::npos)
		{
			host = "0.0.0.0";
			portString = value;
		}
		else
		{
			host = value.substr(0, colon);
			portString = value.substr(colon + 1);
		}
		if (!isValidIpv4Host(host))
			fail("listen host must be an IPv4 address");
		if (!isNumber(portString))
			fail("listen port must be numeric");
		port = parseUnsignedLong(portString, ok);
		if (!ok || port < 1 || port > 65535)
			fail("listen port must be in 1..65535");
		endpoint.host = host;
		endpoint.port = static_cast<unsigned short>(port);
		return (endpoint);
	}

	std::size_t ConfigParser::parseBodySize(const std::string& value) const
	{
		std::string numberPart;
		std::size_t multiplier;
		unsigned long parsed;
		bool ok;
		const std::size_t max = std::numeric_limits<std::size_t>::max();

		if (value.empty())
			fail("size must not be empty");
		multiplier = 1;
		numberPart = value;
		const char suffix = value[value.size() - 1];
		if (suffix == 'K' || suffix == 'k'
			|| suffix == 'M' || suffix == 'm'
			|| suffix == 'G' || suffix == 'g')
		{
			numberPart = value.substr(0, value.size() - 1);
			if (suffix == 'K' || suffix == 'k')
				multiplier = 1024;
			else if (suffix == 'M' || suffix == 'm')
				multiplier = 1024 * 1024;
			else
				multiplier = 1024 * 1024 * 1024;
		}
		if (!isNumber(numberPart))
			fail("size value must be numeric with optional K/M/G suffix");
		parsed = parseUnsignedLong(numberPart, ok);
		if (!ok)
			fail("invalid size value");
		if (static_cast<std::size_t>(parsed) > max / multiplier)
			fail("size value overflow");
		return (static_cast<std::size_t>(parsed) * multiplier);
	}

	int ConfigParser::parseStatusCode(const std::string& value) const
	{
		bool ok;
		unsigned long status;

		if (!isNumber(value))
			fail("status code must be numeric");
		status = parseUnsignedLong(value, ok);
		if (!ok || status < 100 || status > 599)
			fail("status code must be in 100..599");
		return (static_cast<int>(status));
	}

	HttpMethod ConfigParser::parseAllowedMethod(const std::string& value) const
	{
		const HttpMethod method = parseHttpMethod(value);

		if (!isImplementedMethod(method))
			fail("unsupported method in config: " + value);
		return (method);
	}

	bool ConfigParser::parseOnOff(const std::string& value) const
	{
		if (value == "on")
			return (true);
		if (value == "off")
			return (false);
		fail("expected on or off");
		return (false);
	}
}
