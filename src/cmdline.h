#include <optional>
#include <span>
#include <vector>

struct CmdlineArgs {
    bool isHelp{};
    bool isVerbose{};
    bool isScope{};
    const char* slice{};
    std::optional<const char*> workingDir;
    std::vector<const char*> env;
    std::span<const char*> args;  // the element one past the end is guaranteed to be null
};

std::optional<CmdlineArgs> parseArgs(int argc, char* argv[]);
