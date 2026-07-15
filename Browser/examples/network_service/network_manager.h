#pragma once

#include "http_types.h"
#include <future>
#include <string>

class NetworkManager {
public:
    std::future<HttpResponse> sendAsync(HttpRequest request) const;
    void handleResponse(const HttpResponse& response) const;
};
