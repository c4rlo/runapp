#include <format>
#include <print>

inline bool g_verbose;


template<class... Args>
void verbosePrintln(std::format_string<Args...> fmt, Args&&... args)
{
    if (g_verbose) {
        std::println(fmt, std::forward<Args>(args)...);
    }
}

