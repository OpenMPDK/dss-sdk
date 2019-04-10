#ifndef INCLUDED_SMGLOGGER
#define INCLUDED_SMGLOGGER

#include <cstdarg>
#include <memory>
#include <string>

// Samsung Logger implementation
class smgloggerimpl;

// Samsung logger class.
class smglogger
{
public:
    // Constructs a Samsung logger object given a logging category.
    explicit smglogger(std::string const& cat);

    // No copy semantics
    smglogger(smglogger const&); // = delete;

    // No assignment semantics
    smglogger& operator=(smglogger const&); // = delete;

    // Destroys a Samsung logger object.
    ~smglogger();

    // Logs an alert given a format and a list of arguments
    void alert(const char* pszformat, va_list va);

    // Logs a debug message given a format and a list of arguments
    void debug(const char* pszformat, va_list va);

    // Logs an informational message given a format and a list of arguments
    void info(const char* pszformat, va_list va);

    // Logs a warning message given a format and a list of arguments
    void warn(const char* pszformat, va_list va);

    // Logs an error message given a format and a list of arguments
    void error(const char* pszformat, va_list va);

    // Logs a fatal message given a format and a list of arguments
    void fatal(const char* pszformat, va_list va);

private:
    // Pointer to implementation
    std::unique_ptr<smgloggerimpl> pimpl;
};

#endif //INCLUDED_SMGLOGGER
