
#include "Settings.h"
#include <iostream>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

boost::shared_ptr<Settings> const Settings::none;

Settings::Settings(std::string const &name) :
    name_(name),
    type_(dtObject),
    subIterN_((size_t)-1) {
}

Settings::Settings(std::string const &name, std::string const &value) :
    name_(name), 
    type_(dtString),
    subIterN_((size_t)-1),
    value_(value) {
}

boost::shared_ptr<Settings> Settings::load(std::string const &file) {
    std::ifstream ins(file.c_str(), std::ios_base::in);
    if (!ins) {
        return none;
    }
    ins.seekg(0, std::ios_base::end);
    size_t size = ins.tellg();
    ins.seekg(0);
    std::vector<char> vec;
    vec.resize(size + 1);
    ins.read(&vec[0], size);
    vec[size] = 0;

    boost::shared_ptr<Settings> ret(new Settings(file));
    input in(&vec[0], &vec[size]);
    try {
        ret->parse(in);
        if (!in.at_end()) {
            throw std::runtime_error(file + ":" + 
                boost::lexical_cast<std::string>(in.line) + ": Unexpected data after settings");
        }
    }
    catch (std::runtime_error &re) {
        throw std::runtime_error(file + ":" + 
            boost::lexical_cast<std::string>(in.line) + ": " +
            re.what());
    }
    return ret;
}

void Settings::parse(input &in) {
    std::string tok(in.token());
    if (tok == "\"") {
        type_ = dtString;
        value_ = parse_string(in);
    }
    else if (tok == "{") {
        type_ = dtObject;
        parse_object(in);
    }
    else if (tok[0] == '-' || (tok[0] >= '0' && tok[0] <= '9')) {
        if (tok.find('.') != std::string::npos) {
            type_ = dtDouble;
            dValue_ = boost::lexical_cast<double>(tok);
        }
        else {
            type_ = dtLong;
            lValue_ = boost::lexical_cast<long>(tok);
        }
    }
    else {
        throw std::runtime_error(
            "Expected string or object in Settings " + name_);
    }
}

std::string Settings::parse_string(input &in) {
    char const *start = in.pos;
    while (in.pos < in.end) {
        switch (*in.pos) {
            case '\\':
                in.pos++;
                //  quote whatever follows -- if end-of-file, handled by while()
                break;
            case '"':
                in.pos++;
                return std::string(start, in.pos-1);
            default:
                break;
        }
        in.pos++;
    }
    throw std::runtime_error("End of file in string for " + name_);
}

void Settings::parse_object(input &in) {
    type_ = dtObject;
    while (true) {
        std::string tok(in.token());
        if (tok == ",") {
            continue;
        }
        if (tok == "}") {
            return;
        }
        if (tok == "\"") {
            std::string key = parse_string(in);
            if (in.token() != ":") {
                throw std::runtime_error("Expected ':' after key for " + name_);
            }
            boost::shared_ptr<Settings> set(new Settings(key));
            set->parse(in);
            sub_[key] = set;
            subIterN_ = (size_t)-1;
        }
        else {
            throw std::runtime_error("Expected key name or '}' in object for " + name_);
        }
    }
}

std::string const &Settings::get_name() const {
    return name_;
}

bool Settings::is_string() const {
    return type_ == dtString;
}

bool Settings::is_Settings() const {
    return type_ == dtObject;
}

bool Settings::is_long() const {
    return type_ == dtLong;
}

bool Settings::is_double() const {
    return type_ == dtDouble;
}

std::string const &Settings::get_string() const {
    must_be_string("get_string()");
    return value_;
}

long Settings::get_long() const {
    must_be_long("get_long()");
    return lValue_;
}

double Settings::get_double() const {
    must_be_double("get_double()");
    return (is_long() ? lValue_ : dValue_);
}


bool Settings::has_name(std::string const &name) const {
    must_be_Settings("has_name()");
    return sub_.find(name) != sub_.end();
}

boost::shared_ptr<Settings> const &Settings::get_value(std::string const &name) const {
    must_be_Settings("get_value()");
    std::map<std::string, boost::shared_ptr<Settings>>::const_iterator ptr(sub_.find(name));
    if (ptr == sub_.end()) {
        return none;
    }
    return (*ptr).second;
}

size_t Settings::num_names() const {
    must_be_Settings("num_names()");
    return sub_.size();
}

std::string const &Settings::get_name_at(size_t ix) const {
    must_be_Settings("get_name_at()");
    if (ix >= sub_.size()) {
        throw std::runtime_error("Invalid index in Settings::get_name_at()");
    }
    if (ix < subIterN_) {
        subIterN_ = 0;
        subIter_ = sub_.begin();
    }
    while (subIterN_ < ix) {
        ++subIter_;
        ++subIterN_;
    }
    return (*subIter_).first;
}

void Settings::must_be_string(char const *fn) const {
    if (type_ != dtString) {
        throw std::runtime_error(std::string("Must be a string in Settings::") + fn + "; key " + name_);
    }
}

void Settings::must_be_Settings(char const *fn) const {
    if (type_ != dtObject) {
        throw std::runtime_error(std::string("Must be a Settings in Settings::") + fn + "; key " + name_);
    }
}

void Settings::must_be_long(char const *fn) const {
    if (type_ != dtLong) {
        throw std::runtime_error(std::string("Must be an long in Settings::") + fn + "; key " + name_);
    }
}

void Settings::must_be_double(char const *fn) const {
    if (type_ != dtDouble && type_ != dtLong) {
        throw std::runtime_error(std::string("Must be a double in Settings::") + fn + "; key " + name_);
    }
}

Settings::input::input(char const *p, char const *e) :
    pos(p),
    end(e),
    line(1) {
}

std::string Settings::input::token() {
next_line:
    while (pos < end && isspace(*pos)) {
        if (*pos == 10) {
            ++line;
        }
        ++pos;
    }
    /* skip line comments */
    if (*pos == '/' && pos[1] == '/') {
        pos += 2;
        while (pos < end && *pos != 10) {
            ++pos;
        }
        goto next_line;
    }
    /* skip long comments */
    if (*pos == '/' && pos[1] == '*') {
        pos += 2;
        while (pos < end) {
            //  fixme: this stops at end-comment in strings!
            if (*pos == '*' && pos[1] == '/') {
                pos += 2;
                goto next_line;
            }
            if (*pos == 10) {
                ++line;
            }
            ++pos;
        }
    }
    if (*pos == '"' || *pos == '{' || *pos == '}' || *pos == ':' || *pos == ',') {
        pos += 1;
        return std::string(pos - 1, pos);
    }
    else if ((*pos >= '0' && *pos <= '9') || (*pos == '-') || (*pos == '.')) {
        char const *spos = pos;
        ++pos;
        while (pos < end) {
            if ((*pos < '0' || *pos > '9') && 
                !(*pos == '.' || *pos == 'e' || *pos == 'E' || *pos == '-' || *pos == '+')) {
                break;
            }
            ++pos;
        }
        if (pos == spos) {
            throw std::runtime_error("Unexpected numeric token");
        }
        return std::string(spos, pos);
    }
    else {
        throw std::runtime_error("Syntax error in input at '" +
            std::string(pos, pos+1) + "'");
    }
}

bool Settings::input::at_end() {
    while (pos < end && isspace(*pos)) {
        if (*pos == 10) {
            ++line;
        }
        ++pos;
    }
    return pos == end;
}

