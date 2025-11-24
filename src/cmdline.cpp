#include "cmdline.h"

#include <cstdlib>
#include <iostream>
#include <print>
#include <string_view>
#include <utility>

#include <getopt.h>

namespace {

constexpr char UsageStr[] =
    "{0} [OPTIONS] COMMAND...\n"
    "    Run COMMAND as a systemd user unit, in a way suitable for typical applications.\n"
    "    Options:\n"
    "\n"
    "    -v, --verbose: Increase output verbosity.\n"
    "    -o, --scope:   Run command directly, registering it as a systemd scope;\n"
    "                   the default is to run it as a systemd service.\n"
    "    -i SLICE, --slice=SLICE:\n"
    "                   Assign the systemd unit to the given slice (name must include\n"
    "                   \".slice\" suffix); the default is \"app-graphical.slice\".\n"
    "    -d DIR, --dir=DIR:\n"
    "                   Run command in given working directory.\n"
    "    -e VAR=VALUE, --env=VAR=VALUE:\n"
    "                   Run command with given environment variable set;\n"
    "                   may be given multiple times.\n"
    "    -c DESC, --description=DESC:\n"
    "                   Set human-readable unit name (Description= systemd property)\n"
    "                   to given value.\n"
    "\n"
    "{0} --help\n"
    "    Show this help text.\n";

}


std::optional<CmdlineArgs> parseArgs(int argc, char* argv[])
{
    CmdlineArgs args;

    // The '+' prefix makes it so that in a command-line such as
    //     runapp --verbose myprogram --myarg
    // the '--myarg' correctly gets treated as an argument to myprogram,
    // not as an (unknown) argument to runapp.
    // The subsequent ':' makes getopt_long() not print parse errors
    // directly but instead return either '?' or ':' for different kinds
    // of errors.
    const char* shortOptions = "+:voi:d:e:c:";

    const option longOptions[] = {
        { "help",        no_argument,       nullptr, 'h' },
        { "verbose",     no_argument,       nullptr, 'v' },
        { "scope",       no_argument,       nullptr, 'o' },
        { "slice",       required_argument, nullptr, 'i' },
        { "dir",         required_argument, nullptr, 'd' },
        { "env",         required_argument, nullptr, 'e' },
        { "description", required_argument, nullptr, 'c' },
        { }
    };

    const auto shortToLongOption = [&](char c) -> const char* {
        for (const option& o : longOptions) {
            if (o.val == c) {
                return o.name;
            }
        }
        return "<unknown>";
    };

    const auto printUsage = [&]() {
        std::print(std::cout, UsageStr, argv[0]);
    };

    const auto printErr = [&]<class... Args>(std::format_string<Args...> fmt, Args&&... args)
    {
        printUsage();
        // std::clog is like std::cerr but without automatic flushing.
        std::print(std::clog, "\nError: ");
        std::print(std::clog, fmt, std::forward<Args>(args)...);
        std::println(std::cerr, ".");
    };

    int opt{};

    const auto checkAssignOnce = [&](auto& option, const auto& value) {
        if (option) {
            printErr("-{}/--{} may only be given once", char(opt), shortToLongOption(opt));
            return false;
        }
        option = value;
        return true;
    };

    while ((opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            args.isHelp = true;
            break;
        case 'v':
            if (!checkAssignOnce(args.isVerbose, true)) {
                return {};
            }
            break;
        case 'o':
            if (!checkAssignOnce(args.isScope, true)) {
                return {};
            }
            break;
        case 'i':
            if (!checkAssignOnce(args.slice, optarg)) {
                return {};
            }
            if (!std::string_view(args.slice.value()).ends_with(".slice")) {
                printErr("--i/--slice argument must end with \".slice\"");
                return {};
            }
            break;
        case 'd':
            if (!checkAssignOnce(args.workingDir, optarg)) {
                return {};
            }
            break;
        case 'e':
            if (!std::string_view(optarg).contains('=')) {
                printErr("-e/--env argument must be of the form VAR=VALUE");
                return {};
            }
            args.env.push_back(optarg);
            break;
        case 'c':
            if (!checkAssignOnce(args.description, optarg)) {
                return {};
            }
            break;
        case '?':
            if (optopt == 0) {
                printErr("Invalid option: {}", argv[optind - 1]);
            }
            else {
                printErr("Invalid option: -{}", char(optopt));
            }
            return {};
        case ':':
            printErr("Missing argument for option: -{}/--{}",
                     char(optopt), shortToLongOption(char(optopt)));
            return {};
        default:
            printErr("Unknown argument parsing error");
            return {};
        }
    }

    if (args.isHelp) {
        if (optind < argc || args.isVerbose || args.isScope || args.slice
            || args.workingDir || args.description || !args.env.empty())
        {
            printErr("--help may not be combined with any other options or arguments");
            return {};
        }
        printUsage();
        return args;
    }

    if (optind == argc) {
        printErr("Missing command");
        return {};
    }

    if (!args.description) {
        if (const char* envName = std::getenv("DESKTOP_ENTRY_NAME")) {
            args.description = envName;
        }
    }

    args.args = std::span(const_cast<const char**>(&argv[optind]), argc - optind);

    return args;
}
