#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "greeter.grpc.pb.h"

namespace {

class GreeterClient {
public:
    explicit GreeterClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(interview::grpc_hello::Greeter::NewStub(std::move(channel))) {}

    bool SayHello(const std::string& name, uint64_t request_id) {
        interview::grpc_hello::HelloRequest request;
        request.set_name(name);
        request.set_request_id(request_id);

        interview::grpc_hello::HelloReply reply;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));

        const grpc::Status status = stub_->SayHello(&context, request, &reply);
        if (!status.ok()) {
            std::cerr << "SayHello failed: " << status.error_message() << '\n';
            return false;
        }

        std::cout << "Unary reply: " << reply.message()
                  << " (request_id=" << reply.request_id() << ")\n";
        return true;
    }

    bool WatchGreetings(const std::string& name, uint64_t request_id) {
        interview::grpc_hello::HelloRequest request;
        request.set_name(name);
        request.set_request_id(request_id);

        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

        std::unique_ptr<grpc::ClientReader<interview::grpc_hello::HelloReply>> reader =
            stub_->WatchGreetings(&context, request);

        interview::grpc_hello::HelloReply reply;
        while (reader->Read(&reply)) {
            std::cout << "Stream reply: " << reply.message()
                      << " (request_id=" << reply.request_id() << ")\n";
        }

        const grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "WatchGreetings failed: " << status.error_message() << '\n';
            return false;
        }

        return true;
    }

private:
    std::unique_ptr<interview::grpc_hello::Greeter::Stub> stub_;
};

}  // namespace

int main(int argc, char** argv) {
    const std::string address = argc > 1 ? argv[1] : "localhost:50051";
    const std::string name = argc > 2 ? argv[2] : "Duy";

    GreeterClient client(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));

    const bool unary_ok = client.SayHello(name, 1001);
    const bool stream_ok = client.WatchGreetings(name, 1002);

    return unary_ok && stream_ok ? 0 : 1;
}
