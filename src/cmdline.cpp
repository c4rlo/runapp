#include "cmdline.h"

#include <print>

#include <getopt.h>

namespace {

constexpr char usage[] =
    "{} [-v|--verbose] COMMAND...\n"
    "    Run COMMAND as an application under the user systemd instance,\n"
    "    in a way suitable for typical graphical applications.\n"
    "\n"
    "{} --help\n"
    "    Show this help text.\n";

void printUsage(const char* argv0)
{
    std::print(usage, argv0, argv0);
}

}

std::optional<CmdlineArgs> parseArgs(int argc, char* argv[])
{
    CmdlineArgs args;

    // The '+' prefix makes it so that in a command-line such as
    //     runapp --verbose myprogram --myarg
    // the '--myarg' correctly gets treated as an argument to myprogram,
    // not as an (unknown) argument to runapp.
    const char* shortOptions = "+v";

    const option longOptions[] = {
        { "help",    no_argument, nullptr, 'h' },
        { "verbose", no_argument, nullptr, 'v' },
        { }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            printUsage(argv[0]);
            args.help = true;
            return args;
        case 'v':
            args.verbose = true;
            break;
        default:
            std::println("");
            printUsage(argv[0]);
            return {};
        }
    }

    if (optind > argc - 1) {
        printUsage(argv[0]);
        return {};
    }

    args.args = std::span(const_cast<const char**>(&argv[optind]), argc - optind);

    return args;
}
