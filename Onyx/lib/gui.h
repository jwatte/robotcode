
#if !defined(gui_h)
#define gui_h

#include <boost/shared_ptr.hpp>
class Image;

enum {
    HitDirFront = 1,
    HitDirRight = 2,
    HitDirRear = 4,
    HitDirLeft = 8
};
struct GuiState {
    boost::shared_ptr<Image> image;
    float trot;
    unsigned char pose;
    unsigned char hitpoints;
    unsigned char hitdir;
    unsigned char status;
};

class ITime;

void open_gui(GuiState const &state, ITime *it);
void update_gui(GuiState const &state);
void step_gui();
void close_gui();


#endif  //  gui_h

