#if !defined(rl2_PropertyBrowser_h)
#define rl2_PropertyBrowser_h

#include <FL/Fl_Browser_.H>

template<typename T>
class PropertyBrowser : public Fl_Browser_ {
public:
    PropertyBrowser(int x, int y, int w, int h) :
        Fl_Browser_(x, y, w, h),
        count_(0) {
        items_.next = (link *)&items_;
        items_.prev = (link *)&items_;
    }
    ~PropertyBrowser() {
        while (items_.next != (link *)&items_) {
            link *del = items_.next;
            items_.next = del->next;
            delete del;
        }
    }
    void add(T const &value) {
        link *l = new link;
        count_ += 1;
        l->prev = items_.prev;
        items_.prev = l;
        l->next = (link *)&items_;
        l->prev->next = l;
        l->value = value;
        damage(0xff);
    }
protected:

    struct link;
    virtual void *item_first() const {
        return tonull(items_.next);
    }
    virtual void *item_next(void *item) const {
        return tonull(((link *)item)->next);
    }
    virtual void *item_prev(void *item) const {
        return tonull(((link *)item)->prev);
    }
    virtual void *item_last() const {
        return tonull(items_.prev);
    }
    virtual int item_height(void *item) const {
        return ((link *)item)->value.height();
    }
    virtual int item_width(void *item) const {
        return ((link *)item)->value.width();
    }
    virtual int item_quick_height(void *item) const {
        return ((link *)item)->value.height();
    }
    virtual void item_draw(void *item, int x, int y, int w, int h) const {
        ((link *)item)->value.draw(x, y, w, h);
    }
    virtual char const *item_text(void *item) const {
        return ((link *)item)->value.text();
    }
    virtual void item_swap(void *a, void *b) const {
        link *ia = (link *)a;
        link *ib = (link *)b;
        std::swap(ia->value, ib->value);
    }
    virtual void *item_at(int index) const {
        if (index > count_ || index < 1) {
            return NULL;
        }
        if (index < linkCacheCount_) {
            linkCacheCount_ = 1;
            linkCacheItem_ = items_.next;
        }
        while (linkCacheCount_ < index) {
            ++linkCacheCount_;
            linkCacheItem_ = linkCacheItem_->next;
        }
        return linkCacheItem_;
    }
    void *tonull(link *l) const {
        if (l == (link *)&items_) {
            return NULL;
        }
        return l;
    }
    struct linkhdr {
        link *next;
        link *prev;
    };
    struct link : public linkhdr {
        T value;
    };

    mutable int linkCacheCount_;
    mutable link *linkCacheItem_;
    linkhdr items_;
    int count_;
};

#endif  //  rl2_PropertyBrowser_h
