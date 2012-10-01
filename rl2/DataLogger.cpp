
#include "DataLogger.h"
#include "QueuedFile.h"
#include "Settings.h"
#include "ModuleList.h"
#include "Module.h"
#include "Property.h"
#include "str.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <sstream>


DataLogger::DataLogger(
    boost::shared_ptr<ModuleList> const &modules,
    boost::shared_ptr<Settings> const &set) {
    //  wire up a listener set to a set of files
    for (size_t i = 0, n = set->num_names(); i != n; ++i) {
        std::stringstream header;
        boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
        header << "DL01 a " << sizeof(now) << " " << now << " ";
        std::string fn(set->get_name_at(i));
        boost::shared_ptr<QueuedFile> file(QueuedFile::create(fn));
        boost::shared_ptr<Settings> finfo(set->get_value(fn));
        header << finfo->num_names() << " ";
        for (size_t j = 0, m = finfo->num_names(); j != m; ++j) {
            std::string mp(finfo->get_name_at(j));
            std::vector<std::string> modprop;
            split(mp, ':', modprop);
            if (modprop.size() != 2) {
                throw std::runtime_error(
                    "The module:property specification " + mp + " is incorrect (for log file "
                    + fn + ")");
            }
            boost::shared_ptr<Module> mod(modules->get_module_named(modprop[0]));
            boost::shared_ptr<Property> prop(mod->get_property_named(modprop[1]));
            header << "\"" << modprop[0] << ":" << modprop[1] << "\" ";
            header << prop->type();
            header << " ";
            //  set up listener for this property to write to this file
            double reduce = 0;
            maybe_get(finfo, "reduce", reduce);
            
        }
        header << "\n";
        std::string data(header.str());
        file->write(data.c_str(), data.size());
    }
}

DataLogger::~DataLogger() {
    files_.clear();
}


