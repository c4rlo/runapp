#include "dbus.h"

#include <cstdarg>
#include <format>
#include <stdexcept>

#include <string.h>
#include <systemd/sd-bus.h>


namespace {

void check(int rc, const char* operation)
{
    if (rc < 0) {
        throw std::runtime_error(
                std::format("Failed to {}: {} ({})",
                            operation,
                            ::strerror(-rc),
                            ::strerrorname_np(-rc)));
    }
}

}


DBus::DBus(sd_bus* bus)
: d_bus(bus)
{
}

DBus::~DBus()
{
    sd_bus_flush_close_unref(d_bus);
}

DBus DBus::defaultUserBus()
{
    sd_bus* bus{};
    check(sd_bus_default_user(&bus), "create D-Bus connection");
    return DBus(bus);
}

DBusMessage DBus::createMethodCall(
        const char* destination,
        const char* path,
        const char* interface,
        const char* member)
{
    sd_bus_message* msg{};
    check(sd_bus_message_new_method_call(d_bus, &msg, destination, path,
                                         interface, member),
          "create D-Bus method call");
    return DBusMessage(msg);
}

DBusMessage DBus::call(const DBusMessage& message)
{
    sd_bus_message* reply{};
    sd_bus_error error{};
    if (int rc = sd_bus_call(d_bus, message.d_msg, 0, &error, &reply); rc < 0) {
        const std::string exceptionMsg =
                std::format("D-Bus call failed: {}", error.message);
        sd_bus_error_free(&error);
        throw std::runtime_error(exceptionMsg);
    }
    return DBusMessage(reply);
}


DBusMessage::DBusMessage(sd_bus_message* msg)
: d_msg(msg)
{
}

DBusMessage::~DBusMessage()
{
    sd_bus_message_unref(d_msg);
}

void DBusMessage::append(const char* types, ...)
{
    std::va_list args;
    va_start(args, types);
    int rc = sd_bus_message_appendv(d_msg, types, args);
    va_end(args);
    check(rc, "append D-Bus message argument");
}

void DBusMessage::openContainer(char type, const char* contents)
{
    check(sd_bus_message_open_container(d_msg, type, contents),
          "build D-Bus message");
}

void DBusMessage::closeContainer()
{
    check(sd_bus_message_close_container(d_msg), "build D-Bus message");
}
