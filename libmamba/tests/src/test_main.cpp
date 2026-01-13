#include <catch2/catch_session.hpp>

int
main(int argc, char* argv[])
{
    Catch::Session session;

    // Set default test order to declaration order (pre-3.9 behavior)
    // This overrides Catch2 3.9's default random order
    session.configData().runOrder = Catch::TestRunOrder::Declared;

    // Apply command line arguments (which may override the default)
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)
    {
        return returnCode;
    }

    // Run tests
    return session.run();
}
