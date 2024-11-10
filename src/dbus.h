#include <systemd/sd-bus.h>


class DBusMessage;


class DBus {
    sd_bus* d_bus;
    explicit DBus(sd_bus* bus);

  public:
    ~DBus();

    static DBus defaultUserBus();

    DBusMessage createMethodCall(
            const char* destination,
            const char* path,
            const char* interface,
            const char* member);
    DBusMessage call(const DBusMessage& message);
};


class DBusMessage {
    sd_bus_message* d_msg;

    friend DBus;

    explicit DBusMessage(sd_bus_message* msg);

  public:
    ~DBusMessage();
    void append(const char* types, ...);
    void openContainer(char type, const char* contents);
    void closeContainer();
};
