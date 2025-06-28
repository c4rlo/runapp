#include <optional>
#include <span>

struct CmdlineArgs {
    bool help{};
    bool verbose{};
    std::span<const char*> args;
};

std::optional<CmdlineArgs> parseArgs(int argc, char* argv[]);
