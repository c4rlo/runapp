#include <optional>
#include <span>
#include <string>

enum class RunMode { SERVICE, SCOPE };

struct CmdlineArgs {
    bool help{};
    bool verbose{};
    std::string slice{"app-graphical.slice"};
    RunMode runMode{RunMode::SERVICE};
    std::span<const char*> args;  // the element one past the end is guaranteed to be null
};

std::optional<CmdlineArgs> parseArgs(int argc, char* argv[]);
