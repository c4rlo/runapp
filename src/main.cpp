#include "cmdline.h"
#include "dbus.h"
#include "verbose.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <ios>
#include <iostream>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

extern "C" {
#include <sys/pidfd.h>
#include <sys/random.h>
#include <unistd.h>
}


namespace {

namespace fs = std::filesystem;


int throwSystemError(const std::string_view operation, int code)
{
    throw std::system_error(code, std::system_category(),
                            std::format("Failed to {}", operation));
}


void reportSystemError(std::string_view action, int code)
{
    if (code != 0) {
        std::println(std::cerr, "Failed to {}: {}", action,
                     std::error_code(code, std::system_category()).message());
    }
}


struct FdGuard {
    int fd;
    ~FdGuard() {
        if (close(fd) != 0) {
            reportSystemError("close file descriptor", errno);
        }
    }
};


std::optional<std::string> desktopFileID()
{
    if (const char* dfid = std::getenv("FUZZEL_DESKTOP_FILE_ID")) {
        const char ext[] = ".desktop";
        const std::size_t extLen = sizeof(ext) - 1;
        if (std::strlen(dfid) > extLen) {
            const std::size_t dfidLen = std::strlen(dfid);
            if (std::strcmp(dfid + dfidLen - extLen, ext) == 0) {
                return std::string(dfid, dfidLen - extLen);
            }
        }
    }
    return {};
}


DBusMessage buildStartRequest(DBus& bus, const std::string& unitName, const std::string& description,
                              const CmdlineArgs& args)
{
    // Call user systemd via D-Bus. If params.runMode == SERVICE, the call will be
    // equivalent to:
    //
    //   systemd-run --user --unit=${unitName} --description=${description}
    //     --quiet --same-dir --slice=${slice} --collect
    //     --service-type=exec --property=ExitType=cgroup
    //     -- ${argv[1:]}
    //
    // Otherwise (if params.runMode == SCOPE), the call will correspond to:
    //
    //   systemd-run --user --unit=${unitName} --description=${description}
    //     --quiet --same-dir --slice=${slice} --collect
    //     --scope
    //     -- ${argv[1:]}
    //
    // In the latter case, instead of passing ExecStart=, we pass a reference to
    // our own PID in PIDFDs=, and we'll then ultimately execute the target program
    // directly.

    DBusMessage req = bus.createMethodCall(
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "StartTransientUnit");
    req.append("ss", unitName.c_str(), "fail"); // 'name' and 'mode' args

    // Begin unit properties ('properties' arg)
    req.openContainer('a', "(sv)");  // array of struct { key:string, value:variant }
    req.append("(sv)", "Description", "s", description.c_str());
    req.append("(sv)", "CollectMode", "s", "inactive-or-failed");
    req.append("(sv)", "Slice", "s", args.slice);

    if (args.isScope) {
        const int pfd = pidfd_open(getpid(), 0);
        if (pfd == -1) {
            throwSystemError("get pidfd", errno);
        }
        FdGuard pfdGuard{pfd};
        req.append("(sv)", "PIDFDs", "ah", 1, pfd);  // this duplicates the pfd file descriptor
    }
    else {
        req.append("(sv)", "Type", "s", "exec");
        req.append("(sv)", "ExitType", "s", "cgroup");

        if (args.workingDir) {
            req.append("(sv)", "WorkingDirectory", "s",
                       fs::absolute(*args.workingDir).lexically_normal().c_str());
        }

        if (!args.env.empty()) {
            req.openContainer('r', "sv");  // struct { key:string, value:variant }
            req.append("s", "Environment");
            req.openContainer('v', "as");  // variant type: array of string
            req.openContainer('a', "s");   // begin array
            for (const char* env : args.env) {
                req.append("s", env);
            }
            req.closeContainer();  // end array
            req.closeContainer();  // end variant
            req.closeContainer();  // end struct
        }

        // Begin ExecStart= property
        req.openContainer('r', "sv");  // struct { key:string, value:variant }
        req.append("s", "ExecStart");
        req.openContainer('v', "a(sasb)");  // variant type: array of
                                            // { executable:string, argv:array{string}, ignoreFailure:bool }
        req.openContainer('a', "(sasb)");   // begin the above array (which will contain a single element)
        req.openContainer('r', "sasb");     // begin array element struct
        req.append("s", args.args[0]);     // executable
        req.openContainer('a', "s");  // begin argv
        for (const char* arg : args.args) {
            req.append("s", arg);
        }
        req.closeContainer();  // end argv
        req.append("b", 0);    // ignoreFailure = false
        req.closeContainer();  // end array element struct
        req.closeContainer();  // end array
        req.closeContainer();  // end variant
        req.closeContainer();  // end key-value struct
        // End ExecStart= property
    }

    req.closeContainer();
    // End 'properties' arg

    req.append("a(sa(sv))", nullptr); // 'aux' arg is unused

    return req;
}


std::string buildUnitName(const std::string& appName, const CmdlineArgs& args)
{
    // https://systemd.io/DESKTOP_ENVIRONMENTS/#xdg-standardization-for-applications
    // states recommendations that we follow here.

    std::string unitPrefix = "app-";
    if (const char* xdgCurrDesktop = std::getenv("XDG_CURRENT_DESKTOP")) {
        unitPrefix.append(xdgCurrDesktop, ::strchrnul(xdgCurrDesktop, ':') - xdgCurrDesktop);
        unitPrefix += '-';
    }
    unitPrefix += appName;

    // https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html#Description says:
    //   The "unit name prefix" must consist of one or more valid characters (ASCII letters, digits, ":", "-", "_", ".", and "\").
    const auto isInvalidChar = [](char c) {
        return !(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9')
                 || std::strchr(":-_.\\", c) != nullptr);
    };
    std::replace_if(unitPrefix.begin(), unitPrefix.end(), isInvalidChar, '_');

    // https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html#Description says:
    //   The total length of the unit name including the suffix must not exceed 255 characters.
    // We are about to append a random string and a suffix (".service" or ".scope"), so account for that.
    const std::size_t maxPrefixLen = 220;
    if (unitPrefix.size() > maxPrefixLen) {
        unitPrefix.resize(maxPrefixLen);
    }

    std::uint64_t randU64;
    if (::getentropy(&randU64, sizeof randU64) != 0) {
        throwSystemError("get random bytes", errno);
    }

    if (args.isScope) {
        return std::format("{}-{:016x}.scope", unitPrefix, randU64);
    }
    else {
        return std::format("{}@{:016x}.service", unitPrefix, randU64);
    }
}


void startUnit(const std::string& appName, const CmdlineArgs& args)
{
    DBus bus = DBus::systemdUserBus();

    const std::string unitName = buildUnitName(appName, args);
    const DBusMessage req = buildStartRequest(bus, unitName, appName, args);

    // Set up D-Bus signal handlers so we get to know about the result of
    // starting the job

    std::string jobPath, jobResult;

    auto onJobRemoved = bus.createHandler([&jobPath, &jobResult](DBusMessage& msg) {
        const char *sigPath{}, *sigResult{};
        msg.read("uoss", nullptr, &sigPath, nullptr, &sigResult);
        if (jobPath == sigPath) {
            jobResult = sigResult;
        }
    });
    bus.matchSignalAsync("org.freedesktop.systemd1",
                         "/org/freedesktop/systemd1",
                         "org.freedesktop.systemd1.Manager", "JobRemoved",
                         onJobRemoved);

    auto onDisconnected = bus.createHandler([&jobResult](DBusMessage&) {
        if (jobResult.empty()) {
            jobResult = "disconnected";
        }
    });
    bus.matchSignalAsync(
        "org.freedesktop.DBus.Local", nullptr, "org.freedesktop.DBus.Local",
        "Disconnected", onDisconnected);

    if (args.isScope) {
        verbosePrintln("Starting {}; will execute: {}.", unitName, args.args);
    }
    else {
        verbosePrintln("Launching {}: {}.", unitName, args.args);
    }

    auto onStartResponse = bus.createHandler([&jobPath](DBusMessage& resp) {
        const char *path{};
        resp.read("o", &path);
        jobPath = path;
    });
    bus.callAsync(req, onStartResponse);

    bus.driveUntil([&] { return !jobResult.empty(); });

    if (jobResult == "failed") {
        throw std::runtime_error("startup failure");
    }
    else if (jobResult != "done") {
        throw std::runtime_error(jobResult);
    }
}


void executeCommand(const CmdlineArgs& args)
{
    if (args.workingDir) {
        if (chdir(*args.workingDir) != 0) {
            throwSystemError("chdir", errno);
        }
    }
    for (const char* env : args.env) {
        if (putenv(const_cast<char*>(env)) != 0) {
            throwSystemError("putenv", errno);
        }
    }
    execvp(args.args[0], const_cast<char**>(args.args.data()));
    throwSystemError("execute program", errno);
}


void notifyErrorFreedesktop(const std::string& errmsg, const std::optional<std::string>& desktopID)
try {
    DBus bus = DBus::defaultUserBus();

    DBusMessage req = bus.createMethodCall(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "Notify");

    // app_name=null, replaces_id=0, app_icon=null, summary=errmsg,
    // body=null, actions=null
    req.append("susssas", nullptr, 0, nullptr, errmsg.c_str(), nullptr, nullptr);

    // hints
    req.openContainer('a', "{sv}");
    if (desktopID) {
        req.append("{sv}", "desktop-entry", "s", desktopID->c_str());
    }
    req.append("{sv}", "urgency", "y", 2);  // 2=critical
    req.closeContainer();

    // expire_timeout
    req.append("i", 0);  // 0 means never expire

    bool done = false;
    auto onResponse = bus.createHandler([&done](DBusMessage&) { done = true; });
    bus.callAsync(req, onResponse);
    bus.driveUntil([&] { return done; });
}
catch (const std::exception& e) {
    std::println(std::cerr, "Failed to notify user of error via org.freedesktop.Notifications: {}",
                 e.what());
}

} // namespace


int main(int argc, char* argv[])
{
    std::ios::sync_with_stdio(false);  // We only use C++ streams.
    std::cout << std::unitbuf;  // Enable flush after output for cout (like cerr).

    CmdlineArgs args;
    if (auto a = parseArgs(argc, argv)) {
        args = std::move(*a);
    }
    else {
        return 2;
    }

    if (args.isHelp) {
        return 0;
    }

    g_verbose = args.isVerbose;

    const std::optional<std::string> desktopID = desktopFileID();
    const std::string appName =
            desktopID ? *desktopID : fs::path(args.args[0]).filename().string();

    try {
        // Start transient systemd unit (.service or .scope).
        startUnit(appName, args);

        if (args.isScope) {
            // For a scope unit, we now need to execute the command ourselves.
            verbosePrintln("Executing {}.", args.args[0]);
            executeCommand(args);
        }
        else {
            verbosePrintln("Success.");
        }
    }
    catch (const std::exception& e) {
        const std::string errmsg =
                std::format("Failed to start {}: {}", appName, e.what());
        std::println(std::cerr, "{}", errmsg);
        if (!::isatty(STDIN_FILENO)) {
            verbosePrintln("Notifying user of error via org.freedesktop.Notifications.");
            notifyErrorFreedesktop(errmsg, desktopID);
        }
        return 1;
    }
}
