#ifndef WEBSERV_CONFIG_PARSER_HPP
# define WEBSERV_CONFIG_PARSER_HPP

# include "webserv/Config.hpp"
# include <string>
# include <vector>

namespace webserv
{
	class ConfigParser
	{
	public:
		ConfigParser();

		std::vector<ServerConfig>	parseFile(const std::string& path);

	private:
		struct Token
		{
			std::string	value;
			std::size_t	line;

			Token(const std::string& valueValue, std::size_t lineValue);
		};

		std::vector<Token>	_tokens;
		std::size_t			_position;

		ConfigParser(const ConfigParser& other);
		ConfigParser&	operator=(const ConfigParser& other);

		std::string					readFile(const std::string& path) const;
		std::vector<Token>			tokenize(const std::string& content) const;
		std::vector<ServerConfig>	parseServers();
		ServerConfig				parseServerBlock();
		LocationConfig				parseLocationBlock();
		std::vector<std::string>	parseDirectiveArguments();

		bool		atEnd() const;
		const Token&	peek() const;
		const Token&	consume();
		void		expect(const std::string& value);
		void		fail(const std::string& message) const;

		void		applyServerDirective(
						ServerConfig& server,
						const std::string& name,
						const std::vector<std::string>& args,
						bool& listenSet,
						std::vector<std::string>& seen);
		void		applyLocationDirective(
						LocationConfig& location,
						const std::string& name,
						const std::vector<std::string>& args,
						std::vector<std::string>& seen);
		void		validateLocation(const LocationConfig& location) const;

		ListenEndpoint	parseListen(const std::string& value) const;
		std::size_t		parseBodySize(const std::string& value) const;
		int				parseStatusCode(const std::string& value) const;
		HttpMethod		parseAllowedMethod(const std::string& value) const;
		bool			parseOnOff(const std::string& value) const;
	};
}

#endif
