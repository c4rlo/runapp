#include <concepts>
#include <exception>
#include <forward_list>
#include <functional>
#include <memory>

#include <systemd/sd-bus.h>


class DBusMessage;


class DBus {
  public:
    using MessageHandler = std::function<void(DBusMessage&)>;

    static DBus defaultUserBus();

    DBusMessage createMethodCall(
            const char* destination,
            const char* path,
            const char* interface,
            const char* member);

    void callAsync(const DBusMessage& message, MessageHandler handler);

    void matchSignalAsync(
            const char* sender,
            const char* path,
            const char* interface,
            const char* member,
            MessageHandler handler);

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

    struct HandlerData {
        MessageHandler handler;
        DBus* bus;
        bool isOneShot;
    };

    static int handleMessage(sd_bus_message* m, void* userdata, sd_bus_error* retError);

    int handleMessageImpl(sd_bus_message* m, const MessageHandler& handler, sd_bus_error* retError);

    std::unique_ptr<sd_bus, decltype(&sd_bus_flush_close_unref)> d_bus;
    std::forward_list<HandlerData> d_handlers;
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
