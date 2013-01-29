
#include "mwscore.h"
#include "inetwork.h"
#include "util.h"
#include "istatus.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>

static boost::shared_ptr<ISocket> sock;
static IStatus *istatus;
static MWScore theScore;


void do_line(char *str, char *end) {
    MWScore other;
    char const *tok = next(str, end, ':');
    int n;
    if (!tok) goto error;
    other.time = strtod(tok, const_cast<char **>(&tok)) * 0.1;
    tok = next(str, end, ':');
    if (!tok) goto error;
    other.mode = (MWMode)atoi(tok);
    tok = next(str, end, ':');
    if (!tok) goto error;
    n = atoi(tok);
    while (n > 0) {
        char const *name = next(str, end, ':');
        if (!name) goto error;
        char const *hp = next(str, end, ':');
        if (!hp) goto error;
        char const *team = next(str, end, ':');
        if (!team) goto error;
        int tm = atoi(team);
        int h = atoi(hp);
        other.scores[tm][name] = h;
    }
    theScore.scores.swap(other.scores);
    theScore.time = other.time;
    theScore.mode = other.mode;
    return;
error:
    istatus->error("Badly formatted line in mwscore.");
}

void connect_score(char const *addr, unsigned short port, IStatus *status, ISockets *socks) {
    istatus = status;
    sockaddr_in sin;
    unsigned int buf[4];
    unsigned char ucbuf[4];
    if (4 == sscanf(addr, "%d.%d.%d.%d", &buf[0], &buf[1], &buf[2], &buf[3])) {
        if (buf[0] >= 0 && buf[0] <= 255 &&
            buf[1] >= 0 && buf[1] <= 255 &&
            buf[2] >= 0 && buf[2] <= 255 &&
            buf[3] >= 0 && buf[3] <= 255) {
            ucbuf[0] = buf[0] & 0xff;
            ucbuf[1] = buf[1] & 0xff;
            ucbuf[2] = buf[2] & 0xff;
            ucbuf[3] = buf[3] & 0xff;
	    }
        else {
            throw std::runtime_error(std::string("Bad address in connect_score(): ") + addr);
        }
    }
    else {
        throw std::runtime_error(std::string("Wanted x.x.x.x in connect_score(): ") + addr);
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, ucbuf, 4);
    sin.sin_port = htons(port);
    sock = socks->connect(sin);
    sock->send("add me please\n", 14);
    sock->step();
}

bool step_score() {
    if (!sock) {
        return false;
    }
    bool ret = sock->step();
    size_t n;
    char line[2048];
    while ((n = sock->peek(line, 2048)) > 0) {
        char *nl = strchr(line, '\n');
        if (nl == 0) {
            if (n == 2048) {
                istatus->error("Too long line (> 2048 chars) in step_score()");
                sock = boost::shared_ptr<ISocket>();
                return false;
            }
            break;
        }
        *nl = 0;
        do_line(line, nl);
        sock->recvd(nl - line + 1);
    }
    return ret;
}

MWScore &cur_score() {
    return theScore;
}


