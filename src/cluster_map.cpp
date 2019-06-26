#include "cluster_map.h"

/*
Description: Get cluster map information from REST api.
Maximum size of data:
  - Maximum data to be transferred is 16K
  - Max header data 100k
*/

namespace{
    /* ptr   = <points to the delivered data>
    size  = 1 , always 1
    nmemb = <number of byte received>
    userdata = should set with CURLOPT_WRITEDATA
    */
    std::size_t write_callback(char *ptr, size_t size, size_t nmemb, std::string *userdata)
    {
        const std::size_t totalBytes(size * nmemb);
        userdata->append(ptr, totalBytes);
        return totalBytes;
    }
}



/*
Function Name: get_rest_url
Description  : Return the rest URL used to read/write data
Return       : <const std:: string&> , rest URL
*/
const std::string& ClusterMap::get_rest_url()
{
    return URL;
}

/*
Function Name: get_response
Description  : Get the complete JSON response from rest api
Params       : <std::string>  Rest JSON response to be stored into a string.
Return       : <bool> , 0/1 -> Success/Failure
*/
bool ClusterMap::get_response(std::string& response, const long TIMEOUT)
{
    bool good_response = false;
    smg_info(logger, "ClusterMap: Calling REST API %s", URL.c_str());
    // Initialize a curl session
    CURL* curl = curl_easy_init();
    
    if (curl) {
        CURLcode res;
        // Setup remote URL
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());

        // Resolve host name using IPv4-names only.
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        // Timeout connection after TIMEOUT seconds
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);

        // Response code
        long httpCode(0);
        std::unique_ptr<std::string> http_data(new std::string());

        // By default libcurl dumps the output to the default file handler stdout and error to stderr
        // Use a write_callback function to write to a buffer, CURLOPT_WRITEFUNCTION should be used with CURLOPT_WRITEDATA
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        // http_data gets passed to the write_callback as 4th arguments which is userdata.
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, http_data.get());

        // Provide a buffer to store error
        char error_buffer[CURL_ERROR_SIZE];
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer );

        // Follow HTTP redirect if necessary
        //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L)
        // Perform transfer as specified in the options. Its a blocking function.
        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
        {
            // Get return code
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            // Check the response code
            if (httpCode == 200)
            {
                smg_info(logger, "ClusterMap: Received a successful response from Fabric Manager... %u ", httpCode);  // Remove this latter
                response = *http_data.get();
                rest_response = *http_data.get();
                good_response = true;
            }
            else{
                smg_error(logger, "ClusterMap: failed to connect, response code %d", httpCode);
                std::string error_message(*http_data.get());
                smg_error(logger, "ClusterMap: %s", error_message.c_str());
                //smg_error(logger, "ClusterMap: %s", *http_data.get());
                //cout<<*http_data.get() <<endl;
            }
        }
        else{
            // Error handling
            smg_error(logger, "ClusterMap: %s", curl_easy_strerror(res));
        }
        // Clean up session
        curl_easy_cleanup(curl);
    }
    else{
        smg_error(logger, "ClusterMap: Couldn't initialize curl session ...");
    }
    return good_response;
}

/*
Function Name: get_clustermap
Description  : Get Only Clustermap from JSON response.
Params       : <boost::property_tree::ptree>  as cluster map
Return       : <bool> 0/1 -> Success /Failure
*/
bool ClusterMap::get_clustermap(ptree&  nkv_config)
{
    try
    {
        //ptree pt, pt2;
        ptree clustermap;
        std::istringstream is (rest_response);
        read_json(is, clustermap);
        //clustermap = pt.get_child("cm_maps");
        // Add each keys from cluster map to nkv_config
        BOOST_FOREACH(boost::property_tree::ptree::value_type &v, clustermap)
        {
            nkv_config.add_child(v.first.data(), v.second);
        } 

        smg_info(logger, "Updated Subsystem and Cluster information to the NKV configuration ...");
    }
    catch(std::exception const& e)
    {
        smg_error(logger, "ClusterMap: %u", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
