#if !defined(DecisionPanel_h)
#define DecisionPanel_h


class BarGauge;
class Fl_Group;
class Fl_Dial;

struct DecisionPanelData
{
    //  Outputs
    int gas_;
    int turn_;

    //  IR sensors
    int cliff_;
    int leftWheel_;
    int rightWheel_;

    //  US sensors
    int leftWedge_;
    int rightWedge_;
    int backWedge_;
};

class DecisionPanel
{
public:
    DecisionPanel();
    void init();
    void setData(DecisionPanelData const &dpd);

    Fl_Group *group_;
    Fl_Dial *gas_;
    Fl_Dial *turn_;
    BarGauge *cliff_;
    BarGauge *left_;
    BarGauge *right_;
    BarGauge *leftWedge_;
    BarGauge *rightWedge_;
    BarGauge *backWedge_;
};

#endif  //  DecisionPanel_h
