#include<iostream>
#include<memory>
#include<string>
#include<cstdint>
#include<curl/curl.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "csmglogger.h"

using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;
using namespace std;

extern c_smglogger* logger;
//const long TIMEOUT = 10; // 10 seconds

class ClusterMap
{
    const std::string URL;
    std::string rest_response; // rest response
    public:
    ClusterMap(){};
    ClusterMap(std::string rest_url):URL(rest_url) {}
    ~ClusterMap(){};
    bool get_response(std::string& response, const long TIMEOUT=10);
    bool get_clustermap(ptree& clustermap);
    const std::string& get_rest_url();

    //const std::string& get_cluster_status();


};






