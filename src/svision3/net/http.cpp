#include "svision3/net/http.hpp"
#include "svision3/application.hpp"

#include <curl/curl.h>
#include <mutex>
#include <thread>

namespace svision3::net {

namespace {

std::once_flag g_curl_init;

void ensure_curl() {
    std::call_once(g_curl_init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *buf = static_cast<std::string *>(userdata);
    size_t total = size * nmemb;
    buf->append(ptr, total);
    return total;
}

size_t header_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *headers = static_cast<std::vector<std::pair<std::string, std::string>> *>(userdata);
    size_t total = size * nmemb;
    std::string line(ptr, total);

    // Strip trailing \r\n
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    if (line.empty()) {
        return total;
    }

    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim leading whitespace from value
        size_t start = value.find_first_not_of(' ');
        if (start != std::string::npos) {
            value = value.substr(start);
        }
        headers->emplace_back(std::move(key), std::move(value));
    }
    return total;
}

char const *method_string(HttpMethod m) {
    switch (m) {
    case HttpMethod::Get:
        return "GET";
    case HttpMethod::Post:
        return "POST";
    case HttpMethod::Put:
        return "PUT";
    case HttpMethod::Delete:
        return "DELETE";
    }
    return "GET";
}

} // namespace

HttpResponse fetch(HttpRequest const &request) {
    ensure_curl();

    HttpResponse response;
    CURL *curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize curl";
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout_ms));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
#ifdef _WIN32
    // curl implements NATIVE_CA only on Windows; elsewhere OpenSSL finds the
    // distribution's bundle on its own, so the option is not needed there.
    // NATIVE_CA makes it read the OS certificate store.
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif

    if (request.method != HttpMethod::Get) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_string(request.method));
    }

    struct curl_slist *header_list = nullptr;
    for (auto const &[key, value] : request.headers) {
        std::string h = key + ": " + value;
        header_list = curl_slist_append(header_list, h.c_str());
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (!request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
    } else {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        response.status_code = static_cast<int>(code);
    }

    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);
    return response;
}

HttpResponse get(std::string const &url) { return fetch({.url = url}); }

void fetch_async(HttpRequest const &request, std::function<void(HttpResponse)> on_complete) {
    std::thread([request, on_complete = std::move(on_complete)]() mutable {
        auto response = fetch(request);
        svision3::Application::post_to_main_thread(
            [on_complete = std::move(on_complete), response = std::move(response)]() mutable {
                on_complete(std::move(response));
            });
    }).detach();
}

void get_async(std::string const &url, std::function<void(HttpResponse)> on_complete) {
    fetch_async({.url = url}, std::move(on_complete));
}

} // namespace svision3::net
