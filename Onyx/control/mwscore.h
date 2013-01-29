
#if !defined(control_mwscore_h)
#define control_mwscore_h

#include <map>
#include <string>

struct MechScore {
    std::string name;
    int score;
};

enum MWMode {
    MWM_TeamVsTeam = 1,
    MWM_FFA = 2
};

struct MWScore {
    std::map<std::string, MechScore> scores;
    double time;
    MWMode mode;
};

class IStatus;
class ISockets;
class ISocket;

void connect_score(char const *addr, unsigned short port, IStatus *status, ISockets *socks);
bool step_score();  //  returns "connected" or not
MWScore &cur_score();

#endif  //  control_mwscore_h

