#include "cmdline.h"

#include <print>

#include <getopt.h>

namespace {

constexpr char usage[] =
    "runapp usage:\n"
    "\n"
    "{} [-v|--verbose] [-o|--scope] [-i SLICE|--slice=SLICE] COMMAND...\n"
    "    Run COMMAND as an application under the user systemd instance,\n"
    "    in a way suitable for typical graphical applications.\n"
    "\n"
    "    -v, --verbose: Increase output verbosity.\n"
    "    -o, --scope:   Run command directly, registering it as a systemd scope;\n"
    "                   the default is to run it as a systemd service.\n"
    "    -i SLICE, --slice=SLICE:\n"
    "                   Assign the systemd unit to the given slice (name must include\n"
    "                   \".slice\" suffix); the default is \"app-graphical.slice\".\n"
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
    const char* shortOptions = "+voi:";

    const option longOptions[] = {
        { "help",    no_argument,       nullptr, 'h' },
        { "verbose", no_argument,       nullptr, 'v' },
        { "scope",   no_argument,       nullptr, 'o' },
        { "slice",   required_argument, nullptr, 'i' },
        { }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            args.help = true;
            break;
        case 'v':
            args.verbose = true;
            break;
        case 'o':
            args.runMode = RunMode::SCOPE;
            break;
        case 'i':
            args.slice = optarg;
            break;
        default:
            std::println("");
            printUsage(argv[0]);
            return {};
        }
    }

    if (args.help != (optind == argc)) {
        printUsage(argv[0]);
        return {};
    }

    if (args.help) {
        printUsage(argv[0]);
    }

    args.args = std::span(const_cast<const char**>(&argv[optind]), argc - optind);

    return args;
}
