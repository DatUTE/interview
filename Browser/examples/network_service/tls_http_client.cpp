#include "tls_http_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {

struct FdCloser {
    void operator()(int* fd) const {
        if (fd && *fd >= 0) {
            ::close(*fd);
        }
        delete fd;
    }
};

using UniqueFd = std::unique_ptr<int, FdCloser>;

struct AddrInfoDeleter {
    void operator()(addrinfo* info) const {
        if (info) {
            ::freeaddrinfo(info);
        }
    }
};

using UniqueAddrInfo = std::unique_ptr<addrinfo, AddrInfoDeleter>;

struct SslCtxDeleter {
    void operator()(SSL_CTX* ctx) const {
        SSL_CTX_free(ctx);
    }
};

using UniqueSslCtx = std::unique_ptr<SSL_CTX, SslCtxDeleter>;

struct SslDeleter {
    void operator()(SSL* ssl) const {
        SSL_free(ssl);
    }
};

using UniqueSsl = std::unique_ptr<SSL, SslDeleter>;

std::string sslError() {
    unsigned long error = ERR_get_error();
    if (error == 0) {
        return "unknown OpenSSL error";
    }

    char buffer[256];
    ERR_error_string_n(error, buffer, sizeof(buffer));
    return buffer;
}

bool isUnexpectedTlsEof() {
    unsigned long error = ERR_peek_error();
    if (error == 0) {
        return false;
    }
    return ERR_GET_LIB(error) == ERR_LIB_SSL && ERR_GET_REASON(error) == SSL_R_UNEXPECTED_EOF_WHILE_READING;
}

void sslWriteAll(SSL* ssl, const std::string& data) {
    const char* current = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        int written = SSL_write(ssl, current, static_cast<int>(remaining));
        if (written <= 0) {
            throw std::runtime_error("SSL_write failed: " + sslError());
        }
        current += written;
        remaining -= static_cast<size_t>(written);
    }
}

} // namespace

HttpResponse TlsHttpClient::send(const HttpRequest& request) const {
    SSL_library_init();
    SSL_load_error_strings();

    UniqueFd fd(new int(connectTcp(request.host, request.port)));

    UniqueSslCtx ctx(SSL_CTX_new(TLS_client_method()));
    if (!ctx) {
        throw std::runtime_error("SSL_CTX_new failed: " + sslError());
    }
    if (SSL_CTX_set_default_verify_paths(ctx.get()) != 1) {
        throw std::runtime_error("failed to load default CA certificates: " + sslError());
    }
    SSL_CTX_set_verify(ctx.get(), SSL_VERIFY_PEER, nullptr);

    UniqueSsl ssl(SSL_new(ctx.get()));
    if (!ssl) {
        throw std::runtime_error("SSL_new failed: " + sslError());
    }
    if (SSL_set_fd(ssl.get(), *fd) != 1) {
        throw std::runtime_error("SSL_set_fd failed: " + sslError());
    }
    if (SSL_set_tlsext_host_name(ssl.get(), request.host.c_str()) != 1) {
        throw std::runtime_error("SSL_set_tlsext_host_name failed: " + sslError());
    }
    if (SSL_set1_host(ssl.get(), request.host.c_str()) != 1) {
        throw std::runtime_error("SSL_set1_host failed: " + sslError());
    }
    if (SSL_connect(ssl.get()) != 1) {
        throw std::runtime_error("SSL_connect failed: " + sslError());
    }
    if (SSL_get_verify_result(ssl.get()) != X509_V_OK) {
        throw std::runtime_error("TLS certificate verification failed");
    }

    sslWriteAll(ssl.get(), buildWireRequest(request));

    std::string rawResponse;
    char buffer[16 * 1024];
    while (true) {
        int n = SSL_read(ssl.get(), buffer, sizeof(buffer));
        if (n > 0) {
            rawResponse.append(buffer, static_cast<size_t>(n));
            continue;
        }

        int error = SSL_get_error(ssl.get(), n);
        if (error == SSL_ERROR_ZERO_RETURN) {
            break;
        }
        if (error == SSL_ERROR_SYSCALL && n == 0) {
            break;
        }
        if (error == SSL_ERROR_SSL && !rawResponse.empty() && isUnexpectedTlsEof()) {
            ERR_clear_error();
            break;
        }
        throw std::runtime_error("SSL_read failed: " + sslError());
    }

    SSL_shutdown(ssl.get());
    return parseResponse(rawResponse);
}

int TlsHttpClient::connectTcp(const std::string& host, const std::string& port) const {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* rawInfo = nullptr;
    int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &rawInfo);
    if (rc != 0) {
        throw std::runtime_error("getaddrinfo failed: " + std::string(gai_strerror(rc)));
    }
    UniqueAddrInfo info(rawInfo);

    for (addrinfo* current = info.get(); current != nullptr; current = current->ai_next) {
        int fd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (::connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            return fd;
        }
        ::close(fd);
    }

    throw std::runtime_error("TCP connect failed for " + host + ":" + port);
}

std::string TlsHttpClient::buildWireRequest(const HttpRequest& request) const {
    std::ostringstream out;
    out << request.method << " " << request.target << " HTTP/1.1\r\n";
    out << "Host: " << request.host;
    if (request.port != "443") {
        out << ":" << request.port;
    }
    out << "\r\n";
    out << "Connection: close\r\n";
    out << "User-Agent: interview-network-service/1.0\r\n";

    for (const auto& [name, value] : request.headers) {
        out << name << ": " << value << "\r\n";
    }
    if (!request.body.empty()) {
        out << "Content-Length: " << request.body.size() << "\r\n";
    }
    out << "\r\n";
    out << request.body;
    return out.str();
}

HttpResponse TlsHttpClient::parseResponse(const std::string& rawResponse) const {
    HttpResponse response;
    response.raw = rawResponse;

    size_t headerEnd = rawResponse.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        response.body = rawResponse;
        return response;
    }

    std::string headerBlock = rawResponse.substr(0, headerEnd);
    response.body = rawResponse.substr(headerEnd + 4);

    std::istringstream stream(headerBlock);
    std::string statusLine;
    std::getline(stream, statusLine);
    if (!statusLine.empty() && statusLine.back() == '\r') {
        statusLine.pop_back();
    }

    std::istringstream status(statusLine);
    std::string httpVersion;
    status >> httpVersion >> response.statusCode;
    std::getline(status, response.reason);
    if (!response.reason.empty() && response.reason.front() == ' ') {
        response.reason.erase(0, 1);
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        size_t separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        std::string name = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        if (!value.empty() && value.front() == ' ') {
            value.erase(0, 1);
        }
        response.headers[std::move(name)] = std::move(value);
    }

    return response;
}
