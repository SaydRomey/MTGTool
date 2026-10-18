#include "core/http.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace mtg {
namespace {

struct CurlGlobal
{
    CurlGlobal()
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        {
            throw std::runtime_error("curl_global_init failed");
        }
    }
    ~CurlGlobal() { curl_global_cleanup(); }
};

CurlGlobal& curl_global() {
    static CurlGlobal global;
    return global;
}

size_t write_file(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    FILE* file = static_cast<FILE*>(userdata);
    const size_t total = size * nmemb;
    return (std::fwrite(ptr, 1, total, file));
}

struct FileCloser
{
    void operator()(FILE* file) const noexcept
    {
        if (file)
            std::fclose(file);
    }
};

} // namespace

void HttpClient::download_file(const std::string& url,
                               const std::filesystem::path& destination,
                               long timeout_seconds) const
{
    (void)curl_global();

    const auto parent = destination.parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent);
    const auto temp = destination.string() + ".tmp";

    FILE* raw = std::fopen(temp.c_str(), "wb");
    if (!raw)
        throw std::runtime_error("cannot open temporary download file: " + temp);
    std::unique_ptr<FILE, FileCloser> file(raw);

    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("curl_easy_init failed");
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl, &curl_easy_cleanup);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "mtgtool/0.1 (+local personal project)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file.get());
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    file.reset();

    if (rc != CURLE_OK)
    {
        std::filesystem::remove(temp);
        throw std::runtime_error(std::string("download failed: ") + curl_easy_strerror(rc));
    }
    if (status < 200 || status >= 300)
    {
        std::filesystem::remove(temp);
        throw std::runtime_error("download failed with HTTP status " + std::to_string(status));
    }

    std::error_code ec;
    std::filesystem::remove(destination, ec);
    std::filesystem::rename(temp, destination);
}

} // namespace mtg
