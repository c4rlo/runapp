#include "dbus.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <sys/random.h>
#include <unistd.h>


namespace {

namespace fs = std::filesystem;

const char FUZZEL_DFID_ENV[] = "FUZZEL_DESKTOP_FILE_ID";


int raiseError(const std::string_view operation, int rc)
{
    throw std::system_error(rc, std::system_category(),
                            std::format("Failed to {}", operation));
}


void run(DBus& bus, const std::string& appName, std::span<const char*> args)
{
    // Determine systemd unit name
    std::uint64_t randU64;
    if (::getentropy(&randU64, sizeof randU64) != 0) {
        raiseError("get random bytes", errno);
    }
    std::string dashDE;
    if (const char* xdgCurrDesktop = std::getenv("XDG_CURRENT_DESKTOP")) {
        dashDE.push_back('-');
        dashDE.append(xdgCurrDesktop, ::strchrnul(xdgCurrDesktop, ':') - xdgCurrDesktop);
    }
    const std::string unitName =
        std::format("app{}-{}@{:016x}.service", dashDE, appName, randU64);

    // Get current working directory
    const fs::path cwd = fs::current_path();

    // Call user systemd via D-Bus. Our call will be equivalent to:
    //
    //   systemd-run --user --unit=${unitName} --description=${appName} --service-type=exec
    //     --property=ExitType=cgroup --same-dir --slice=app-graphical.slice --collect
    //     --quiet -- ${argv[1:]}

    DBusMessage req = bus.createMethodCall(
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "StartTransientUnit");
    req.append("ss", unitName.c_str(), "fail"); // 'name' and 'mode' args

    // Begin unit properties ('properties' arg)
    req.openContainer('a', "(sv)");  // array of struct { key:string, value:variant }
    req.append("(sv)", "Description", "s", appName.c_str());
    req.append("(sv)", "CollectMode", "s", "inactive-or-failed");
    req.append("(sv)", "ExitType", "s", "cgroup");
    req.append("(sv)", "Slice", "s", "app-graphical.slice");
    req.append("(sv)", "Type", "s", "exec");
    req.append("(sv)", "WorkingDirectory", "s", cwd.c_str());

    // Begin ExecStart= property
    req.openContainer('r', "sv");  // struct { key:string, value:variant }
    req.append("s", "ExecStart");
    req.openContainer('v', "a(sasb)");  // variant type: array of
                                        // { executable:string, argv:array{string}, ignoreFailure:bool }
    req.openContainer('a', "(sasb)");   // begin the above array (which will contain a single element)
    req.openContainer('r', "sasb");     // begin array element struct
    req.append("s", args[0]);     // executable
    req.openContainer('a', "s");  // begin argv
    for (const char* arg : args) {
        req.append("s", arg);
    }
    req.closeContainer();  // end argv
    req.append("b", 0);    // ignoreFailure
    req.closeContainer();  // end array element struct
    req.closeContainer();  // end array
    req.closeContainer();  // end variant
    req.closeContainer();  // end key-value struct
    // End ExecStart= property

    req.closeContainer();
    // End 'properties' arg

    req.append("a(sa(sv))", nullptr); // 'aux' arg is unused

    // Set up D-Bus signal handlers so we get to know about the result of
    // starting the job

    std::string jobPath, jobResult;

    auto onJobRemoved = [&jobPath, &jobResult](DBusMessage& msg) {
        const char *sigPath{}, *sigResult{};
        msg.read("uoss", nullptr, &sigPath, nullptr, &sigResult);
        if (jobPath == sigPath) {
            jobResult = sigResult;
        }
    };
    bus.matchSignalAsync("org.freedesktop.systemd1",
                         "/org/freedesktop/systemd1",
                         "org.freedesktop.systemd1.Manager", "JobRemoved",
                         std::move(onJobRemoved));

    auto onDisconnected = [&jobResult](DBusMessage&) {
        if (jobResult.empty()) {
            jobResult = "disconnected";
        }
    };
    bus.matchSignalAsync(
        "org.freedesktop.DBus.Local", nullptr, "org.freedesktop.DBus.Local",
        "Disconnected", std::move(onDisconnected));

    auto onStartResponse = [&jobPath](DBusMessage &resp) {
        const char *path{};
        resp.read("o", &path);
        jobPath = path;
    };
    bus.callAsync(req, std::move(onStartResponse));

    bus.driveUntil([&jobResult] { return !jobResult.empty(); });

    if (jobResult == "failed") {
        throw std::runtime_error("startup failure");
    }
    else if (jobResult != "done") {
        throw std::runtime_error(jobResult);
    }
}

void notifyError(DBus& bus, const std::string& desktopID, const std::string& errmsg)
try {
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
    if (!desktopID.empty()) {
        req.append("{sv}", "desktop-entry", "s", desktopID.c_str());
    }
    req.append("{sv}", "urgency", "y", 2);  // 2=critical
    req.closeContainer();

    // expire_timeout
    req.append("i", 0);  // 0 means never expire

    bool done = false;
    bus.callAsync(req, [&done](DBusMessage&) { done = true; });
    bus.driveUntil([&done] { return done; });
}
catch (const std::exception& e) {
    std::println("Failed to notify user of error: {}", e.what());
}

} // namespace


int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::println(std::cerr, "usage: {} COMMAND...", argv[0]);
        return 2;
    }

    std::string desktopID;
    if (const char* dfid = std::getenv(FUZZEL_DFID_ENV)) {
        const char ext[] = ".desktop";
        const std::size_t extLen = sizeof(ext) - 1;
        if (std::strlen(dfid) > extLen) {
            const std::size_t dfidLen = std::strlen(dfid);
            if (std::strcmp(dfid + dfidLen - extLen, ext) == 0) {
                desktopID.assign(dfid, dfidLen - extLen);
            }
        }
    }

    std::string appName;
    if (!desktopID.empty()) {
        appName = desktopID;
    }
    else {
        appName = fs::path(argv[1]).filename();
    }

    std::optional<DBus> bus;
    try {
        bus = DBus::defaultUserBus();
        run(*bus, appName,
            std::span(const_cast<const char**>(&argv[1]), argc - 1));
        return 0;
    }
    catch (const std::exception& e) {
        const std::string errmsg =
                std::format("Failed to start {}: {}", appName, e.what());
        std::println(std::cerr, "{}", errmsg);
        if (bus && !::isatty(STDIN_FILENO)) {
            notifyError(*bus, desktopID, errmsg);
        }
        return 1;
    }
}
