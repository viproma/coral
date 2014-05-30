#include "model_parser.hpp"

#include <cassert>

#include "boost/foreach.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/property_tree/ptree.hpp"


namespace
{
    // Parses an INFO file as a property tree.
    boost::property_tree::ptree ReadPtreeInfoFile(const std::string& path)
    {
        boost::property_tree::ptree pt;
        boost::property_tree::read_info(path, pt);
        return pt;
    }
}


dsb::model::Model ParseModelFile(const std::string& path,
                                 dsb::library::Library& library)
{
    using namespace dsb::model;
    const auto ptree = ReadPtreeInfoFile(path);
    Model model;
    BOOST_FOREACH (const auto& slaveNode,
                   ptree.get_child("slaves", boost::property_tree::ptree())) {
        const auto& slaveName = slaveNode.first;
        const auto& slaveData = slaveNode.second;
        const auto& slaveTypeName = slaveData.get<std::string>("type");
        auto slaveType = library.FindSlaveType(slaveTypeName);
        if (slaveType == nullptr) {
            throw std::runtime_error(path + ": Unknown slave type: "
                                     + slaveTypeName);
        }
        model.AddSlave(slaveName, *slaveType);
        BOOST_FOREACH (const auto& initNode,
                       slaveData.get_child("init", boost::property_tree::ptree())) {
            try {
                model.SetVariableFromString(slaveName, initNode.first, initNode.second.data());
            } catch (const std::out_of_range&) {
                throw std::runtime_error("Invalid variable name: " + slaveName
                                         + "." + initNode.first);
            } catch (const std::bad_cast&) {
                throw std::runtime_error("Invalid value for variable " + slaveName
                                         + "." + initNode.first + ": "
                                         + initNode.second.data());
            }
        }
    }
    BOOST_FOREACH (const auto& connNode,
                   ptree.get_child("connections", boost::property_tree::ptree())) {
        model.Connect(VariableID(connNode.first), VariableID(connNode.second.data()));
    }
    return model;
}
