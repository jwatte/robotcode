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
    std::string const &get_string() const;

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
    void parse(input &in);
    void parse_object(input &in);
    std::string parse_string(input &in);
    std::string name_;
    bool isString_;
    std::map<std::string, boost::shared_ptr<Settings>> sub_;
    mutable std::map<std::string, boost::shared_ptr<Settings>>::const_iterator subIter_;
    mutable size_t subIterN_;
    std::string value_;
    static boost::shared_ptr<Settings> const none;
};

#endif  //  rl2_Settings_h

