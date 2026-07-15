#include "network_manager.h"

#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    std::string method = argc >= 2 ? argv[1] : "GET";
    std::string host = argc >= 3 ? argv[2] : "example.com";
    std::string target = argc >= 4 ? argv[3] : "/";
    std::string port = argc >= 5 ? argv[4] : "443";
    std::string body = argc >= 6 ? argv[5] : "";

    try {
        NetworkManager networkManager;

        HttpRequestBuilder builder;
        builder.method(method)
            .host(host)
            .port(port)
            .target(target)
            .header("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

        if (!body.empty()) {
            builder.header("Content-Type", "application/json").body(body);
        }

        HttpRequest request = builder.build();

        std::future<HttpResponse> future = networkManager.sendAsync(std::move(request));

        HttpResponse response = future.get();
        networkManager.handleResponse(response);
    } catch (const std::exception& ex) {
        std::cerr << "network_service_demo failed: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
