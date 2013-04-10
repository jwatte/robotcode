
#if !defined(rl2_Scoreboard_h)
#define rl2_Scoreboard_h

#include <string>
#include <logger.h>

class ITime;


enum ScoreType {
    ScoreTypeInt = 1,       //  int
    ScoreTypeDouble = 2,    //  double
    ScoreTypeString = 3     //  std::string
};

template<typename T> struct score_info;
template<> struct score_info<int> {
    enum { typenum = ScoreTypeInt };
};
template<> struct score_info<double> {
    enum { typenum = ScoreTypeDouble };
};

template<> struct score_info<std::string> {
    enum { typenum = ScoreTypeString };
};


class IScore {
public:
    virtual char const *name() = 0;
    virtual ScoreType type() = 0;
    virtual unsigned int count() = 0;
    virtual LogKey key() = 0;
    virtual void *value() = 0;
    virtual void set_value(void const *base, unsigned int count) = 0;
    virtual double last_update() = 0;
protected:
    virtual ~IScore() {}
};

class IScoreboard {
public:
    virtual IScore *get_name(char const *name) = 0;
    virtual IScore *get_or_make_name(char const *name, ScoreType type, unsigned int count = 1, LogKey key = LogKeyNull) = 0;
    virtual IScore *get_index(unsigned int index) = 0;
    virtual unsigned int count() = 0;
    virtual ~IScoreboard() {}
};

IScoreboard *mk_scoreboard(char const *name, ITime *time);

#endif  //  rl2_Scoreboard_h

