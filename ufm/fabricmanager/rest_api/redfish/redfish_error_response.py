from common.ufmlog import ufmlog
from common.utils.ufm_decorators import singleton
from rest_api.redfish import redfish_constants


@singleton
class RedfishErrorResponse:
    def __init__(self):
        self.log = ufmlog.log(module="RFDB", mask=ufmlog.UFM_REDFISH_DB)

    def get_server_error_response(self, exception):
        self.log.exception(exception)
        return {"Status": redfish_constants.SERVER_ERROR,
                "Message": redfish_constants.SERVER_ERROR_STR}

    def get_method_not_allowed_response(self, message):
        self.log.error(f'Method not supported for {message}')
        return {"Status": redfish_constants.METHOD_NOT_ALLOWED,
                "Message": redfish_constants.METHOD_NOT_ALLOWED_STR}
