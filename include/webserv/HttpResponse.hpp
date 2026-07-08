#ifndef WEBSERV_HTTP_RESPONSE_HPP
# define WEBSERV_HTTP_RESPONSE_HPP

# include <string>
# include <utility>
# include <vector>

namespace webserv
{
	class HttpResponse
	{
	public:
		HttpResponse();

		void	setVersion(const std::string& version);
		void	setStatusCode(int statusCode);
		void	setReasonPhrase(const std::string& reasonPhrase);
		void	setHeader(const std::string& name, const std::string& value);
		void	setBody(const std::string& body);
		void	setConnectionClose(bool closeConnection);

		const std::string&	version() const;
		int					statusCode() const;
		const std::string&	reasonPhrase() const;
		const std::string&	body() const;
		bool				connectionClose() const;

		std::string	serialize() const;

	private:
		typedef std::pair<std::string, std::string> Header;

		std::string			_version;
		int					_statusCode;
		std::string			_reasonPhrase;
		std::vector<Header>	_headers;
		std::string			_body;
		bool				_connectionClose;

		bool	hasHeader(const std::string& name) const;
		bool	statusMustNotHaveBody() const;
	};
}

#endif
