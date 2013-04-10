
#include <Scoreboard.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <new>
#include <logger.h>
#include <boost/foreach.hpp>
#include <itime.h>



static unsigned int type_size(ScoreType type) {
    switch (type) {
    case ScoreTypeInt: return sizeof(int);
    case ScoreTypeDouble: return sizeof(double);
    case ScoreTypeString: return sizeof(std::string);
    default: throw std::runtime_error("Bad type in type_size()");
    }
}

static void empty_construct(ScoreType type, void *dst) {
    switch (type) {
    case ScoreTypeInt: new(dst) int(0);
    case ScoreTypeDouble: new(dst) double(0);
    case ScoreTypeString: new(dst) std::string("");
    default: throw std::runtime_error("Bad type in empty_construct()");
    }
}

static void copy_construct(ScoreType type, void const *src, void *dst) {
    switch (type) {
    case ScoreTypeInt: new(dst) int(*(int const*)src);
    case ScoreTypeDouble: new(dst) double(*(double const *)src);
    case ScoreTypeString: new(dst) std::string(*(std::string const *)src);
    default: throw std::runtime_error("Bad type in copy_construct()");
    }
}

using std::string;

static void deconstruct(ScoreType type, void *dst) {
    switch (type) {
    case ScoreTypeInt: /*((int *)dst)->~int();*/ return;
    case ScoreTypeDouble: /*((double *)dst)->~double();*/ return;
    case ScoreTypeString: ((string *)dst)->~string(); return;
    default: throw std::runtime_error("Bad type in deconstruct()");
    }
}

class Score : public IScore {
public:
    Score(std::string const &name, ScoreType type, unsigned int count, LogKey key, ITime *time) :
        name_(name),
        type_(type),
        count_(count),
        key_(key),
        time_(time),
        lastUpdate_(time_->now())
    {
        if (count_ == 0 || count_ > 1024) {
            throw std::runtime_error("Bad array count in Score()");
        }
        valueSpace_ = new char[type_size(type_) * count_];
        char *dst = valueSpace_;
        for (unsigned int i = 0; i != count; ++i) {
            empty_construct(type_, dst);
            dst += type_size(type_);
        }
    }
    ~Score() {
        delete[] valueSpace_;
        char *dst = valueSpace_;
        for (unsigned int i = 0; i != count_; ++i) {
            deconstruct(type_, dst);
            dst += type_size(type_);
        }
    }
    char const *name() { return name_.c_str(); }
    ScoreType type() { return type_; }
    unsigned int count() { return count_; }
    LogKey key() { return key_; }
    void *value() { return valueSpace_; }
    void set_value(void const *base, unsigned int count) {
        if (count != count_) {
            throw std::runtime_error("Inconsistent count in Score::set_value()");
        }
        char *dst = valueSpace_;
        char const *src = (char const *)base;
        for (unsigned int i = 0; i != count; ++i) {
            copy_construct(type_, src, dst);
            dst += type_size(type_);
            src += type_size(type_);
        }
        lastUpdate_ = time_->now();
    }
    double last_update() {
        return lastUpdate_;
    }

    std::string name_;
    ScoreType type_;
    unsigned int count_;
    LogKey key_;
    ITime *time_;
    double lastUpdate_;
    char *valueSpace_;
};

class Scoreboard;

static std::map<std::string, Scoreboard *> &get_map() {
    static std::map<std::string, Scoreboard *> boards_;
    return boards_;
}

class Scoreboard : public IScoreboard {
public:
    Scoreboard(std::string const &name, ITime *time) :
        name_(name),
        time_(time)
    {
        if (get_map().find(name_) != get_map().end()) {
            throw std::runtime_error("Duplicate scoreboard instance: " + name_);
        }
        get_map()[name] = this;
    }

    IScore *get_name(char const *name) {
        auto ptr(scoreMap_.find(name));
        if (ptr == scoreMap_.end()) {
            return nullptr;
        }
        return scores_[(*ptr).second];
    }

    IScore *get_or_make_name(char const *name, ScoreType type, unsigned int count = 1, LogKey key = LogKeyNull) {
        IScore *ret = get_name(name);
        if (ret != nullptr) {
            Score *r = static_cast<Score *>(ret);
            if (r->type_ != type || r->count_ != count || r->key_ != key) {
                throw std::runtime_error("Inconsistent type/count/key in Score::get_or_make_name()");
            }
            return ret;
        }
        Score *rr = new Score(name, type, count, key, time_);
        scoreMap_[rr->name_] = scores_.size();
        scores_.push_back(rr);
        return rr;
    }

    IScore *get_index(unsigned int index) {
        if (index >= scores_.size()) {
            throw std::runtime_error("Invalid index in Scoreboard::get_index()");
        }
        return scores_[index];
    }

    unsigned int count() {
        return scores_.size();
    }

    std::map<std::string, int> scoreMap_;
    std::vector<Score *> scores_;
    std::string name_;
    ITime *time_;

    ~Scoreboard() {
        get_map().erase(get_map().find(name_));
        BOOST_FOREACH(auto x, scores_) {
            delete x;
        }
    }

};



IScoreboard *mk_scoreboard(char const *name, ITime *time) {
    return new Scoreboard(name, time);
}

