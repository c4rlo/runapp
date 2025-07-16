#include <optional>
#include <span>
#include <vector>

struct CmdlineArgs {
    bool isHelp{};
    bool isVerbose{};
    bool isScope{};
    // The following 'const char*' pointers all point into the argv,
    // hence they never go out of scope.
    const char* slice{};
    std::optional<const char*> workingDir;
    std::vector<const char*> env;
    std::span<const char*> args;  // the element one past the end is guaranteed to be null
};

std::optional<CmdlineArgs> parseArgs(int argc, char* argv[]);
