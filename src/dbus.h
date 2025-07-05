#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <vector>

#include <systemd/sd-bus.h>


class DBusHandler;
class DBusMessage;

using DBusMessageFunc = std::function<void(DBusMessage&)>;


class DBus {
  public:
    // Return connection to user systemd instance via the standard D-Bus broker.
    static DBus defaultUserBus();

    // Return connection to user systemd instance via dedicated systemd-provided
    // socket, bypassing the D-Bus broker, for better performance.
    static DBus systemdUserBus();

    DBusMessage createMethodCall(
            const char* destination,
            const char* path,
            const char* interface,
            const char* member);

    DBusHandler createHandler(DBusMessageFunc&& handler);

    void callAsync(const DBusMessage& message, const DBusHandler& handler);

    void matchSignalAsync(
            const char* sender,
            const char* path,
            const char* interface,
            const char* member,
            const DBusHandler& handler);

    void drive();

    void driveUntil(const std::invocable auto& condition)
    {
        while (!condition()) {
            drive();
        }
    }

  private:
    explicit DBus(sd_bus* bus) noexcept;

    void setException(std::exception_ptr e);

    static int handleMessage(sd_bus_message* m, void* userdata, sd_bus_error* retError);

    int handleMessageImpl(sd_bus_message* m, const DBusMessageFunc& handler, sd_bus_error* retError);

    std::unique_ptr<sd_bus, decltype(&sd_bus_flush_close_unref)> d_bus;
    std::exception_ptr d_exception;
};


class DBusMessage {
  public:
    void read(const char* types, ...);

    void append(const char* types, ...);
    void openContainer(char type, const char* contents);
    void closeContainer();

  private:
    explicit DBusMessage(sd_bus_message* msg) noexcept;

    std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> d_msg;

    friend DBus;
};


class DBusHandler {
  private:
    DBusHandler(DBusMessageFunc&& handler, DBus* bus);

    struct Impl {
        DBusMessageFunc d_handler;
        DBus* d_bus;
        std::vector<sd_bus_slot*> d_slots;

        ~Impl();

        void checkBusIs(DBus* bus);
    };

    std::unique_ptr<Impl> d_impl;

    friend DBus;
};
