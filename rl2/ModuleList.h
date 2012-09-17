#if !defined(rl2_ModuleList_h)
#define rl2_ModuleList_h

#include "ListenableImpl.h"
#include "Module.h"
#include "cast_as.h"

class ModuleList : public ListenableImpl<cast_as_impl<Listenable, ModuleList>> {
public:
    static boost::shared_ptr<ModuleList> create();
    void add(boost::shared_ptr<Module> m);
    void remove(boost::shared_ptr<Module> m);
    size_t num_modules();
    boost::shared_ptr<Module> get_module_at(size_t ix);
    bool contains(boost::shared_ptr<Module> m);
    void step_all();
private:
    std::vector<boost::shared_ptr<Module>> modules_;
};

#endif  //  rl2_ModuleList_h
