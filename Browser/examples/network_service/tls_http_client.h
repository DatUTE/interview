#pragma once

#include "http_types.h"

#include <string>

class TlsHttpClient {
public:
    HttpResponse send(const HttpRequest& request) const;

private:
    int connectTcp(const std::string& host, const std::string& port) const;
    std::string buildWireRequest(const HttpRequest& request) const;
    HttpResponse parseResponse(const std::string& rawResponse) const;
};
