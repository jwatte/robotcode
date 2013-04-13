#if !defined(rl2_Settings_h)
#define rl2_Settings_h

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>
#include <map>

class Settings : public boost::enable_shared_from_this<Settings> {
public:
    static boost::shared_ptr<Settings> load(std::string const &file);

    std::string const &get_name() const;
    bool is_string() const;
    bool is_Settings() const;
    bool is_long() const;
    bool is_double() const;

    std::string const &get_string() const;
    long get_long() const;
    double get_double() const;

    bool has_name(std::string const &name) const;
    boost::shared_ptr<Settings> const &get_value(std::string const &name) const;
    size_t num_names() const;
    std::string const &get_name_at(size_t ix) const;

private:

    struct input {
        input(char const *p, char const *end);
        std::string token();
        bool at_end();
        char const *pos;
        char const *end;
        int line;
    };
    Settings(std::string const &name);
    Settings(std::string const &name, std::string const &value);
    void must_be_string(char const *fn) const;
    void must_be_Settings(char const *fn) const;
    void must_be_long(char const *fn) const;
    void must_be_double(char const *fn) const;
    void parse(input &in);
    void parse_object(input &in);
    std::string parse_string(input &in);
    std::string name_;
    enum DataType {
        dtObject,
        dtString,
        dtLong,
        dtDouble
    };
    DataType type_;
    std::map<std::string, boost::shared_ptr<Settings>> sub_;
    mutable std::map<std::string, boost::shared_ptr<Settings>>::const_iterator subIter_;
    mutable size_t subIterN_;
    std::string value_;
    long lValue_;
    double dValue_;
    static boost::shared_ptr<Settings> const none;
};

template<typename T> inline void get(boost::shared_ptr<Settings> const &set, T &oval);

template<> inline void get<long>(boost::shared_ptr<Settings> const &set, long &oval) {
    oval = set->get_long();
}

template<> inline void get<double>(boost::shared_ptr<Settings> const &set, double &oval) {
    oval = set->get_double();
}

template<> inline void get<std::string>(boost::shared_ptr<Settings> const &set, std::string &oval) {
    oval = set->get_string();
}

template<typename T>
inline bool maybe_get(boost::shared_ptr<Settings> const &set, std::string const &name, T &oval) {
    if (!set) return false;
    auto v = set->get_value(name);
    if (!!v) {
        get(v, oval);
        return true;
    }
    return false;
}

template<typename T, typename D>
inline bool maybe_get(boost::shared_ptr<Settings> const &set, std::string const &name, T &oval, D const &dflt) {
    if (!set) return false;
    auto v = set->get_value(name);
    if (!!v) {
        get(v, oval);
        return true;
    }
    oval = dflt;
    return false;
}

#endif  //  rl2_Settings_h

