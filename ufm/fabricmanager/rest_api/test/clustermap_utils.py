def get_transport_type(addr):
    if 'oem' not in addr or 'SupportedProtocol' not in addr['oem']:
        protocol = 'TCP'
    else:
        protocol = addr.oem.SupportedProtocol
    if 'oem' not in addr or 'Port' not in addr['oem']:
        port = 1024
    else:
        port = addr.oem.Port
    return protocol, port


def get_percent_available(storage):
    if 'oem' not in storage or 'PercentAvailable' not in storage['oem']:
        percent_available = 0
    else:
        percent_available = storage.oem.PercentAvailable
    return percent_available
