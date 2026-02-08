#include <Application.h>

int
main(int argc, char** argv)
{
  auto buildInfo = "1.0.0 (" + std::string(GIT_COMMIT_SHORT) + ")";
  if constexpr (GIT_DIRTY) {
    buildInfo += " [dirty]";
  }

  mdns::engine::Application app(1280, 720, buildInfo);
  app.run();
  return 0;
}
