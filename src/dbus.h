#include <systemd/sd-bus.h>

#include <exception>
#include <forward_list>
#include <functional>


class DBusMessage;


class DBus {
  public:
    using MessageHandler = std::function<void(DBusMessage&)>;

    ~DBus();

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

  private:
    explicit DBus(sd_bus* bus);

    void setException(std::exception_ptr e);

    struct HandlerData {
        MessageHandler handler;
        DBus* bus{};
        bool isOneShot;
    };

    static int handleMessage(sd_bus_message* m, void* userdata, sd_bus_error* retError);
    static int handleMessageImpl(sd_bus_message* m, HandlerData* h, sd_bus_error* retError);

    sd_bus* d_bus;
    std::forward_list<HandlerData> d_handlers;
    std::exception_ptr d_exception;
};


class DBusMessage {
  public:
    ~DBusMessage();

    void read(const char* types, ...);

    void append(const char* types, ...);
    void openContainer(char type, const char* contents);
    void closeContainer();

  private:
    explicit DBusMessage(sd_bus_message* msg);

    sd_bus_message* d_msg;

    friend DBus;
};
