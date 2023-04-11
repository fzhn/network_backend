#include "network.h"
// extern "C" {
//     #include "examples/ucp_util.h"
// }
#include "ucp/api/ucp.h"



NetworkUCX::NetworkUCX(network_config& config) : Network(config){
    /* UCP temporary vars */
    ucp_params_t ucp_params;
    ucp_worker_params_t worker_params;
    ucp_config_t* uconfig;
    ucs_status_t status;

    /* UCP handler objects */
    ucp_context_h ucp_context;
    ucp_worker_h ucp_worker;

    status = ucp_config_read(NULL, NULL, &uconfig);

    ucp_params.field_mask   = UCP_PARAM_FIELD_FEATURES |
                              UCP_PARAM_FIELD_REQUEST_SIZE |
                              UCP_PARAM_FIELD_REQUEST_INIT;
    ucp_params.features     = UCP_FEATURE_TAG;

    status = ucp_init(&ucp_params, uconfig, &ucp_context);

    ucp_config_print(uconfig, stdout, NULL, UCS_CONFIG_PRINT_CONFIG);
}
                                            
NetworkUCX::~NetworkUCX(){
    
}

void NetworkUCX::establish_oob_connection(network_handle* handle){

}






void NetworkUCX::run(){}
void NetworkUCX::register_handle(network_handle* handle){}
void NetworkUCX::close_handle(network_handle* handle){}
ssize_t NetworkUCX::send_data(const char* data, size_t size, network_handle* handle){return 0;}