#include "webserv/ConfigParser.hpp"
#include "webserv/Server.hpp"
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	const char* kDefaultConfigPath = "config/default.conf";

	const char* programName(char** argv)
	{
		if (argv != 0 && argv[0] != 0 && argv[0][0] != '\0')
			return (argv[0]);
		return ("webserv");
	}

	int printUsage(const char* name)
	{
		std::cerr << "Usage: " << name << " [configuration file]" << std::endl;
		return (1);
	}

	std::string selectConfigPath(int argc, char** argv)
	{
		if (argc == 1)
			return (std::string(kDefaultConfigPath));
		return (std::string(argv[1]));
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc < 1 || argc > 2)
			return (printUsage(programName(argv)));

		const std::string configPath = selectConfigPath(argc, argv);

		std::cout << "webserv STEP08 config parser server" << std::endl;
		std::cout << "config: " << configPath << std::endl;

		webserv::ConfigParser parser;
		const std::vector<webserv::ServerConfig> servers =
			parser.parseFile(configPath);

		std::cout << "parsed servers: " << servers.size() << std::endl;
		std::cout << "using listen: " << servers[0].listen[0].host
				  << ":" << servers[0].listen[0].port << std::endl;
		std::cout << "using root: " << servers[0].root << std::endl;
		std::cout << "locations: " << servers[0].locations.size() << std::endl;

		webserv::Server server(servers[0]);
		server.run();
		return (0);
	}
	catch (const std::exception& error)
	{
		std::cerr << "webserv: fatal: " << error.what() << std::endl;
		return (1);
	}
	catch (...)
	{
		std::cerr << "webserv: fatal: unknown exception" << std::endl;
		return (1);
	}
}
