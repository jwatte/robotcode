#if !defined(rl2_DataLogger_h)
#define rl2_DataLogger_h

#include <boost/shared_ptr.hpp>
#include <string>
#include <map>

class ModuleList;
class Settings;
class QueuedFile;

class DataLogger {
public:
    DataLogger(
        boost::shared_ptr<ModuleList> const &modules,
        boost::shared_ptr<Settings> const &set);
    virtual ~DataLogger();

private:

    std::map<std::string, boost::shared_ptr<QueuedFile>> files_;
};

#endif  //  rl2_DataLogger_h
