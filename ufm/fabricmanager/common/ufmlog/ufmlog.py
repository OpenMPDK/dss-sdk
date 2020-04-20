
import logging

'''
UFM Module bit masks
    - Simply assign a bit for a unique grouping of module log messages
'''
UFM_MAIN       = 0x00000001
UFM_DB         = 0x00000010
UFM_REDFISH_DB = 0x00000020
UFM_MQ         = 0x00000040
UFM_MONITOR    = 0x00010000
UFM_CONTROLLER = 0x00020000
UFM_COLLECTOR  = 0x00040000
UFM_POLLER     = 0x00080000
UFM_NKV        = 0x00100000

UFM_ALL        = 0xFFFFFFFF
UFM_NONE       = 0x00000000

# Defualt UFM Log file
UFM_LOG_FILE  = 'ufm.log'


# Handles global managment of log
class Ufmlog(object):
    def __init__(self, filename=UFM_LOG_FILE, log_info=UFM_ALL, log_error=UFM_ALL,
            log_warning=UFM_ALL, log_debug=UFM_NONE, log_detail=UFM_NONE):
        self.filename = filename
        self.level = logging.DEBUG
        self.format = '%(asctime)s %(message)s'
        self.datefmt = '%Y/%m/%d %H:%M:%S' # This format can be sorted as text
        self.count = 0
        self.log_info = log_info
        self.log_error = log_error
        self.log_warning = log_warning
        self.log_debug = log_debug
        self.log_detail = log_detail

    def __del__(self):
        self.count = 0

    def open(self):
        if self.count == 0:
            logging.basicConfig(filename=self.filename, level=0, format=self.format, datefmt=self.datefmt)
            logging.info('UFMLOG:')
            logging.info('UFMLOG: Opened')
            self.count = 1
        else:
            self.count += 1

    def close(self):
        if self.count == 1:
            self.count = 0
        else:
            self.count -= 1

    def set_log_info(self, mask):
        self.log_info = mask

    def get_log_info(self):
        return (self.log_info)

    def set_log_debug(self, mask):
        self.log_debug = mask

    def get_log_debug(self):
        return (self.log_debug)

    def set_log_warning(self, mask):
        self.log_warning = mask

    def get_log_warning(self):
        return (self.log_warning)

    def set_log_error(self, mask):
        self.log_error = mask

    def get_log_error(self):
        return (self.log_error)

    def set_log_detail(self, mask):
        self.log_detail = mask

    def get_log_detail(self):
        return (self.log_detail)


# Global ufmlog object
ufmlog = Ufmlog()


# Module level managment of log
class log(object):
    def __init__(self, module='UFM', mask=int(UFM_MAIN)):
        self.module = module
        self.mask = int(mask)
        self.ufmlog = ufmlog
        ufmlog.open()
        logging.info(self.module+': Logging Started.')

    def __del__(self):
        ufmlog.close()

    def info(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_info) != 0:
            logging.info(self.module+': INFO: '+msg, *args, **kwargs)

    def detail(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_detail) != 0:
            logging.info(self.module+': DETAIL: '+msg, *args,**kwargs)

    def debug(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_debug) != 0:
            logging.info(self.module+': DEBUG: '+msg, *args, **kwargs)

    def warning(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_warning) != 0:
            logging.info(self.module+': WARNING: '+msg, *args, **kwargs)

    def error(self, msg, *args, **kwargs):
        if (self.mask & ufmlog.log_error) != 0:
            logging.info(self.module+': ERROR: '+msg, *args, **kwargs)

    def exception(self, excep):
        logging.info(self.module+': EXCEPTION:')
        logging.exception(excep)


# INFO

    def set_log_info(self, mask):
        ufmlog.set_log_info(mask)

    def get_log_info(self):
        return (ufmlog.get_log_info())

    def log_info_on(self):
        ufmlog.log_info = ufmlog.log_info | self.mask

    def log_info_off(self):
        ufmlog.log_info = ufmlog.log_info & ~self.mask

# DEBUG

    def set_log_debug(self, mask):
        ufmlog.set_log_debug(mask)

    def get_log_debug(self):
        return (ufmlog.get_log_debug())

    def log_debug_on(self):
        ufmlog.log_debug = ufmlog.log_debug | self.mask

    def log_debug_off(self):
        ufmlog.log_debug = ufmlog.log_debug & ~self.mask

# WARNING

    def set_log_warning(self, mask):
        ufmlog.set_log_warning(mask)

    def get_log_warning(self):
        return (ufmlog.get_log_warning())

    def log_warning_on(self):
        ufmlog.log_warning = ufmlog.log_warning | self.mask

    def log_warning_off(self):
        ufmlog.log_warning = ufmlog.log_warning & ~self.mask

# ERROR

    def set_log_error(self, mask):
        ufmlog.set_log_error(mask)

    def get_log_error(self):
        return (ufmlog.get_log_error())

    def log_error_on(self):
        ufmlog.log_error = ufmlog.log_error | self.mask

    def log_error_off(self):
        ufmlog.log_error = ufmlog.log_error & ~self.mask

# DETAIL

    def set_log_detail(self, mask):
        ufmlog.set_log_detail(mask)

    def get_log_detail(self):
        return (ufmlog.get_log_detail())

    def log_detail_on(self):
        ufmlog.log_detail = ufmlog.log_detail | self.mask

    def log_detail_off(self):
        ufmlog.log_detail = ufmlog.log_detail & ~self.mask
