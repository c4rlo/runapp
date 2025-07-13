#include "cmdline.h"

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
    "                   Set working directory of command to DIR.\n"
    "    -e VAR=VALUE, --env=VAR=VALUE:\n"
    "                   Run command with given environment variable set;\n"
    "                   may be given multiple times.\n"
    "\n"
    "{0} --help\n"
    "    Show this help text.\n";

constexpr char DefaultSlice[] = "app-graphical.slice";

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
    const char* shortOptions = "+:voi:d:e:";

    const option longOptions[] = {
        { "help",    no_argument,       nullptr, 'h' },
        { "verbose", no_argument,       nullptr, 'v' },
        { "scope",   no_argument,       nullptr, 'o' },
        { "slice",   required_argument, nullptr, 'i' },
        { "dir",     required_argument, nullptr, 'd' },
        { "env",     required_argument, nullptr, 'e' },
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

    const auto printErrOptionOnce = [&]() {
        printErr("-{}/--{} may only be given once", char(opt), shortToLongOption(opt));
    };

    while ((opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            args.isHelp = true;
            break;
        case 'v':
            if (args.isVerbose) {
                printErrOptionOnce();
                return {};
            }
            args.isVerbose = true;
            break;
        case 'o':
            if (args.isScope) {
                printErrOptionOnce();
                return {};
            }
            args.isScope = true;
            break;
        case 'i':
            if (args.slice) {
                printErrOptionOnce();
                return {};
            }
            if (!std::string_view(optarg).ends_with(".slice")) {
                printErr("--i/--slice argument must end with \".slice\"");
                return {};
            }
            args.slice = optarg;
            break;
        case 'd':
            if (args.workingDir) {
                printErrOptionOnce();
                return {};
            }
            args.workingDir = optarg;
            break;
        case 'e':
            if (!std::string_view(optarg).contains('=')) {
                printErr("-e/--env argument must be of the form VAR=VALUE");
                return {};
            }
            args.env.push_back(optarg);
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
        if (optind < argc || args.isVerbose || args.isScope || args.slice || args.workingDir
            || !args.env.empty())
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

    if (!args.slice) {
        args.slice = DefaultSlice;
    }

    args.args = std::span(const_cast<const char**>(&argv[optind]), argc - optind);

    return args;
}
