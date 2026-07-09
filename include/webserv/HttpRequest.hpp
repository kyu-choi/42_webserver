#ifndef WEBSERV_HTTP_REQUEST_HPP
# define WEBSERV_HTTP_REQUEST_HPP

# include "webserv/Http.hpp"
# include <map>
# include <string>

namespace webserv
{
	class HttpRequest
	{
	public:
		enum BodyFraming
		{
			BODY_NONE,
			BODY_CONTENT_LENGTH,
			BODY_CHUNKED
		};

		HttpRequest();

		void	clear();

		void	setMethodText(const std::string& methodText);
		void	setMethod(HttpMethod method);
		void	setRawTarget(const std::string& rawTarget);
		void	setPath(const std::string& path);
		void	setQuery(const std::string& query);
		void	setVersion(const std::string& version);
		void	addHeader(const std::string& name, const std::string& value);
		void	setBody(const std::string& body);
		void	setBodyFraming(BodyFraming framing);
		void	setContentLength(std::size_t contentLength);
		void	setErrorStatus(int status);

		const std::string&	methodText() const;
		HttpMethod			method() const;
		const std::string&	rawTarget() const;
		const std::string&	path() const;
		const std::string&	query() const;
		const std::string&	version() const;
		const std::string&	body() const;
		BodyFraming			bodyFraming() const;
		bool				hasContentLength() const;
		std::size_t			contentLength() const;
		const std::map<std::string, std::string>&	headers() const;
		bool				hasHeader(const std::string& name) const;
		std::string			header(const std::string& name) const;
		int					errorStatus() const;

	private:
		std::string							_methodText;
		HttpMethod							_method;
		std::string							_rawTarget;
		std::string							_path;
		std::string							_query;
		std::string							_version;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		BodyFraming							_bodyFraming;
		bool								_hasContentLength;
		std::size_t							_contentLength;
		int									_errorStatus;
	};
}

#endif
