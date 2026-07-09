#ifndef WEBSERV_UPLOAD_HANDLER_HPP
# define WEBSERV_UPLOAD_HANDLER_HPP

# include "webserv/HttpRequest.hpp"
# include "webserv/HttpResponse.hpp"
# include <string>
# include <vector>

namespace webserv
{
	class UploadHandler
	{
	public:
		explicit UploadHandler(const std::string& uploadStore);

		HttpResponse	handlePost(const HttpRequest& request) const;

	private:
		struct SavedFile
		{
			std::string	filename;
			bool		created;

			SavedFile(const std::string& filenameValue, bool createdValue);
		};

		std::string	_uploadStore;

		HttpResponse	handleRawBody(const HttpRequest& request) const;
		HttpResponse	handleMultipart(
							const HttpRequest& request,
							const std::string& boundary) const;
		HttpResponse	buildSuccessResponse(
							const std::vector<SavedFile>& savedFiles) const;
		bool			ensureUploadStore() const;
		bool			saveFile(
							const std::string& filename,
							const std::string& content,
							bool& created) const;
	};
}

#endif
