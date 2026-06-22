#include <grpcpp/grpcpp.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "greeter.grpc.pb.h"

namespace {

class GreeterService final : public interview::grpc_hello::Greeter::Service {
public:
    grpc::Status SayHello(grpc::ServerContext* context,
                          const interview::grpc_hello::HelloRequest* request,
                          interview::grpc_hello::HelloReply* reply) override {
        (void)context;

        if (request->name().empty()) {
            return {grpc::StatusCode::INVALID_ARGUMENT, "name must not be empty"};
        }

        reply->set_message("Hello, " + request->name() + "!");
        reply->set_request_id(request->request_id());
        return grpc::Status::OK;
    }

    grpc::Status WatchGreetings(
        grpc::ServerContext* context,
        const interview::grpc_hello::HelloRequest* request,
        grpc::ServerWriter<interview::grpc_hello::HelloReply>* writer) override {
        if (request->name().empty()) {
            return {grpc::StatusCode::INVALID_ARGUMENT, "name must not be empty"};
        }

        for (int i = 1; i <= 3; ++i) {
            if (context->IsCancelled()) {
                return {grpc::StatusCode::CANCELLED, "client cancelled stream"};
            }

            interview::grpc_hello::HelloReply reply;
            reply.set_message("Hello #" + std::to_string(i) + ", " + request->name() + "!");
            reply.set_request_id(request->request_id());

            if (!writer->Write(reply)) {
                return {grpc::StatusCode::UNAVAILABLE, "stream write failed"};
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        return grpc::Status::OK;
    }
};

void RunServer(const std::string& address) {
    GreeterService service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server) {
        throw std::runtime_error("failed to start gRPC server");
    }

    std::cout << "Greeter server listening on " << address << '\n';
    server->Wait();
}

}  // namespace

int main(int argc, char** argv) {
    const std::string address = argc > 1 ? argv[1] : "0.0.0.0:50051";

    try {
        RunServer(address);
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
