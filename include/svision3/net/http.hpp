#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace svision3::net {

enum class HttpMethod { Get, Post, Put, Delete };

struct HttpRequest {
    std::string url;
    HttpMethod method = HttpMethod::Get;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeout_ms = 10000;
};

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string error;

    bool ok() const { return error.empty() && status_code >= 200 && status_code < 300; }
};

/// Synchronous HTTP fetch -- safe to call from any thread.
HttpResponse fetch(HttpRequest const &request);

/// Convenience GET.
HttpResponse get(std::string const &url);

/// Async fetch: runs on a background thread, delivers result on the main
/// thread via Application::post_to_main_thread.
void fetch_async(HttpRequest const &request,
                 std::function<void(HttpResponse)> on_complete);

/// Async convenience GET.
void get_async(std::string const &url,
               std::function<void(HttpResponse)> on_complete);

} // namespace svision3::net
