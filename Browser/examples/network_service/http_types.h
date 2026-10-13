#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

struct HttpRequest {
    std::string method = "GET";
    std::string host;
    std::string port = "443";
    std::string target = "/";
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int statusCode = 0;
    std::string reason;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string raw;
};

class HttpRequestBuilder {
public:
    HttpRequestBuilder& method(std::string value) {
        m_request.method = std::move(value);
        return *this;
    }

    HttpRequestBuilder& host(std::string value) {
        m_request.host = std::move(value);
        return *this;
    }

    HttpRequestBuilder& port(std::string value) {
        m_request.port = std::move(value);
        return *this;
    }

    HttpRequestBuilder& target(std::string value) {
        m_request.target = std::move(value);
        return *this;
    }

    HttpRequestBuilder& header(std::string name, std::string value) {
        m_request.headers[std::move(name)] = std::move(value);
        return *this;
    }

    HttpRequestBuilder& body(std::string value) {
        m_request.body = std::move(value);
        return *this;
    }

    HttpRequest build() const {
        if (m_request.method.empty()) {
            throw std::invalid_argument("HTTP method must not be empty");
        }
        if (m_request.host.empty()) {
            throw std::invalid_argument("HTTP host must not be empty");
        }
        if (m_request.port.empty()) {
            throw std::invalid_argument("HTTP port must not be empty");
        }
        if (m_request.target.empty() || m_request.target.front() != '/') {
            throw std::invalid_argument("HTTP target must start with '/'");
        }
        return m_request;
    }

private:
    HttpRequest m_request;
};
