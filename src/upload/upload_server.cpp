#include <upload/UploadServer.h>

int main() {
    std::string port = "8002";

    util::initLogging();

    edge::UploadServer server;
    server.listen(port);

    return 0;
}