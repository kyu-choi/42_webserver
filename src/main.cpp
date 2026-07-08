#include "webserv/Server.hpp"
#include <exception>
#include <iostream>
#include <string>

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

		std::cout << "webserv STEP06 HTTP response builder server" << std::endl;
		std::cout << "config: " << configPath << std::endl;
		std::cout << "note: config parsing starts in a later step" << std::endl;

		webserv::Server server("127.0.0.1", 8080);
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
