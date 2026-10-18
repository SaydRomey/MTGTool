#pragma once

#include <filesystem>
#include <string>

namespace mtg {

class HttpClient {
public:
    void download_file(const std::string& url,
                       const std::filesystem::path& destination,
                       long timeout_seconds = 180) const;
};

} // namespace mtg
