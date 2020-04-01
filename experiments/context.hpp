#pragma once

#include <vector>
#include <string>

// Context singleton class
class Context
{
public:

	std::vector<std::string> pkgs_dirs = {"$CONDA_PREFIX/pkgs"};

    Context(Context const&) = delete;
    Context& operator=(Context const&) = delete;

    static std::shared_ptr<Context> instance()
    {
        static std::shared_ptr<Context> s{new Context};
        return s;
    }

private:
    Context() {}
};