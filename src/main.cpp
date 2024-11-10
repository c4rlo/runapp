#include "dbus.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <system_error>

#include <stdlib.h>
#include <sys/random.h>


namespace {

namespace fs = std::filesystem;

const char DFID_ENV_NAME[] = "FUZZEL_DESKTOP_FILE_ID";


int raiseError(const char* operation, int rc)
{
    throw std::system_error(rc, std::system_category(),
                            std::format("Failed to {}", operation));
}


void run(std::span<const char*> args)
{
    // Get current working directory
    const fs::path cwd = fs::current_path();

    // Determine app name
    std::string appName;
    if (const char* dfid = std::getenv(DFID_ENV_NAME)) {
        const char ext[] = ".desktop";
        const std::size_t extLen = sizeof(ext) - 1;
        if (std::strlen(dfid) > extLen) {
            const std::size_t dfidLen = std::strlen(dfid);
            if (std::strcmp(dfid + dfidLen - extLen, ext) == 0) {
                appName.assign(dfid, dfidLen - extLen);
            }
        }
        ::unsetenv(DFID_ENV_NAME);
    }
    if (appName.empty()) {
        appName = fs::path(args[0]).filename();
    }

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

    // Call user systemd via D-Bus. Our call will be equivalent to:
    //
    //   systemd-run --user --unit=${unitName} --description=${appName} --service-type=exec
    //     --property=ExitType=cgroup --same-dir --slice=app-graphical.slice --collect
    //     --no-block --quiet -- ${argv[1:]}

    DBus bus = DBus::defaultUserBus();
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

    // Call systemd, ignoring response
    bus.call(req);
}

} // namespace



int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::println(std::cerr, "usage: {} COMMAND...", argv[0]);
        return 2;
    }

    try {
        run(std::span(const_cast<const char**>(&argv[1]), argc - 1));
        return 0;
    }
    catch (const std::exception& e) {
        std::println(std::cerr, "{}", e.what());
        return 1;
    }
}
