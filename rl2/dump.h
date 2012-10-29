
#if !defined(rl2_dump_h)
#define rl2_dump_h

#include <boost/foreach.hpp>

template<typename T> inline void dump(T &t) {
    std::cerr << "DUMP: ";
    for (auto p = t.begin(), e = t.end(); p != e; ++p) {
        std::cerr << *p << ", ";
    }
    std::cerr << std::endl;
}

#include "ModuleList.h"

template<> inline void dump(ModuleList &ml) {
    std::cerr << "DUMP: ";
    for (size_t i = 0, n = ml.num_modules(); i != n; ++i) {
        boost::shared_ptr<Module> sp(ml.get_module_at(i));
        std::cerr << sp->name() << ", ";
    }
    std::cerr << std::endl;
}

#include "Module.h"
#include "Property.h"

template<> inline void dump(Module &m) {
    std::cerr << "DUMP: ";
    for (size_t i = 0, n = m.num_properties(); i != n; ++i) {
        boost::shared_ptr<Property> sp(m.get_property_at(i));
        std::cerr << sp->name() << ", ";
    }
    std::cerr << std::endl;
}


#endif  //  rl2_dump_h

