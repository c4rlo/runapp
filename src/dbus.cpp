#include "dbus.h"

#include <cstdarg>
#include <cstdint>
#include <format>
#include <print>
#include <stdexcept>
#include <utility>

#include <systemd/sd-bus.h>


namespace {

void check(int rc, const char* operation)
{
    if (rc < 0) {
        throw std::system_error(-rc, std::system_category(),
                                std::format("Failed to {}", operation));
    }
}

}


DBus::DBus(sd_bus* bus) noexcept
: d_bus(bus, sd_bus_flush_close_unref)
{
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
    check(sd_bus_message_new_method_call(d_bus.get(), &msg, destination, path,
                                         interface, member),
          "create D-Bus method call");
    return DBusMessage(msg);
}

void DBus::callAsync(const DBusMessage& message, MessageHandler handler)
{
    d_handlers.emplace_front(std::move(handler), this, true);
    check(sd_bus_call_async(d_bus.get(), nullptr, message.d_msg.get(),
                            handleMessage, &d_handlers.front(), 0),
          "install D-Bus method response handler");
}

void DBus::matchSignalAsync(
        const char* sender,
        const char* path,
        const char* interface,
        const char* member,
        MessageHandler handler)
{
    d_handlers.emplace_front(std::move(handler), this, false);
    check(sd_bus_match_signal_async(d_bus.get(), nullptr, sender, path,
                                    interface, member, handleMessage, nullptr,
                                    &d_handlers.front()),
          "install D-Bus signal handler");
}

void DBus::drive()
{
    while (true) {
        int rc = sd_bus_process(d_bus.get(), nullptr);
        check(rc, "process D-Bus messages");
        if (d_exception) {
            std::exception_ptr e;
            std::swap(e, d_exception);
            std::rethrow_exception(e);
        }
        if (rc > 0) {
            return;
        }
        check(sd_bus_wait(d_bus.get(), UINT64_MAX), "wait for D-Bus messages");
    }
}

void DBus::setException(std::exception_ptr e)
{
    if (!d_exception) {
        d_exception = std::move(e);
    }
    else {
        try {
            std::rethrow_exception(e);
        }
        catch (const std::exception& exc) {
            std::println("Additional error: {}", exc.what());
        }
        catch (...) {
            std::println("Additional error (unknown)");
        }
    }
}

int DBus::handleMessage(sd_bus_message* m, void* userdata, sd_bus_error* retError)
{
    auto* h = static_cast<HandlerData*>(userdata);
    int rc = h->bus->handleMessageImpl(m, h->handler, retError);
    if (h->isOneShot) {
        h->bus->d_handlers.remove_if(
                [h](const HandlerData& hd) { return &hd == h; });
    }
    return rc;
}

int DBus::handleMessageImpl(sd_bus_message* m, const MessageHandler& handler, sd_bus_error* retError)
{
    if (sd_bus_message_is_method_error(m, nullptr)) {
        const sd_bus_error* err = sd_bus_message_get_error(m);
        setException(std::make_exception_ptr(std::runtime_error(err->message)));
        return sd_bus_error_copy(retError, err);
    }

    try {
        DBusMessage msg{sd_bus_message_ref(m)};
        handler(msg);
        return 0;
    }
    catch (const std::exception& e) {
        setException(std::current_exception());
        return sd_bus_error_set(retError, "runapp.Error", e.what());
    }
    catch (...) {
        setException(std::current_exception());
        return sd_bus_error_set(retError, "runapp.Error", "Unknown error");
    }
}


DBusMessage::DBusMessage(sd_bus_message* msg) noexcept
: d_msg(msg, sd_bus_message_unref)
{
}

void DBusMessage::read(const char* types, ...)
{
    std::va_list args;
    va_start(args, types);
    int rc = sd_bus_message_readv(d_msg.get(), types, args);
    va_end(args);
    check(rc, "read D-Bus message field");
    if (rc == 0) {
        throw std::runtime_error("Failed to read D-Bus message field: EOF");
    }
}

void DBusMessage::append(const char* types, ...)
{
    std::va_list args;
    va_start(args, types);
    int rc = sd_bus_message_appendv(d_msg.get(), types, args);
    va_end(args);
    check(rc, "append D-Bus message field");
}

void DBusMessage::openContainer(char type, const char* contents)
{
    check(sd_bus_message_open_container(d_msg.get(), type, contents),
          "build D-Bus message (open container)");
}

void DBusMessage::closeContainer()
{
    check(sd_bus_message_close_container(d_msg.get()),
          "build D-Bus message (close container)");
}
