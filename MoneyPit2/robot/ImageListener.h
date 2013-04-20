#if !defined(robot_ImageListener_h)
#define robot_ImageListener_h

class ImageListener : public Listener {
public:
    ImageListener(boost::shared_ptr<Property> const &prop) :
        prop_(prop),
        dirty_(false)
    {
    }
    void on_change() {
        image_ = prop_->get<boost::shared_ptr<Image>>();
        dirty_ = true;
    }
    bool check_and_clear() {
        bool ret = dirty_;
        dirty_ = false;
        return ret;
    }
    boost::shared_ptr<Property> prop_;
    boost::shared_ptr<Image> image_;
    bool dirty_;
};


#endif  //  robot_ImageListener_h
