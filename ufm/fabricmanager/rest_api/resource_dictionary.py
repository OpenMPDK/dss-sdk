resource_dict = {}

class ResourceDictionary():
    """
    Helper class to add/get resources
    """

    def __init__(self):
        print('Initializing Resource Dictionary')

    def get_resource(self, path):
        config = resource_dict[path].configuration
        return config

    def add_resource(self, path, static_loader_obj):
        resource_dict[path] = static_loader_obj
        return static_loader_obj
