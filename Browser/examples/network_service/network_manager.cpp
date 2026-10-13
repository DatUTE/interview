#include "network_manager.h"

#include "thread_pool.h"
#include "tls_http_client.h"

#include <algorithm>
#include <iostream>
#include <utility>

std::future<HttpResponse> NetworkManager::sendAsync(HttpRequest request) const {
    return ThreadPool::instance().submit([request = std::move(request)]() {
        TlsHttpClient client;
        return client.send(request);
    });
}

void NetworkManager::handleResponse(const HttpResponse& response) const {
    std::cout << "HTTP status: " << response.statusCode << " " << response.reason << "\n";

    auto contentType = response.headers.find("content-type");
    if (contentType != response.headers.end()) {
        std::cout << "Content-Type: " << contentType->second << "\n";
    }

    size_t previewSize = std::min<size_t>(response.body.size(), 400);
    std::cout << "Body bytes: " << response.body.size() << "\n";
    std::cout << "Body preview:\n";
    std::cout << response.body.substr(0, previewSize) << "\n";
}
