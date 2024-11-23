#include <systemd/sd-bus.h>

#include <functional>
#include <memory>
#include <vector>


class DBusMessage;


class DBus {
  public:
    using SignalCallback = std::function<void(DBusMessage&)>;

  private:
    sd_bus* d_bus;
    std::vector<std::unique_ptr<SignalCallback>> d_callbacks;

    DBus(const DBus&) = delete;

  public:
    explicit DBus(sd_bus* bus);
    ~DBus();

    static DBus defaultUserBus();

    DBusMessage createMethodCall(
            const char* destination,
            const char* path,
            const char* interface,
            const char* member);
    DBusMessage call(const DBusMessage& message);

    void matchSignal(
            const char* sender,
            const char* path,
            const char* interface,
            const char* member,
            SignalCallback callback);

    void processAndWait();
};


class DBusMessage {
    sd_bus_message* d_msg;

    friend DBus;

    DBusMessage(const DBusMessage&) = delete;

  public:
    explicit DBusMessage(sd_bus_message* msg);
    ~DBusMessage();

    void read(const char* types, ...);

    void append(const char* types, ...);
    void openContainer(char type, const char* contents);
    void closeContainer();
};
