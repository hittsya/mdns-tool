#include <Application.h>

int main(int argc, char **argv)
{
    // mdns::MdnsHelper helper;
    // helper.runDiscovery();

    mdns::engine::Application app(1280, 720, "Test");
    app.run();

    return 0;
}
