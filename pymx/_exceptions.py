# Brief  : File containing custom exceptions for mx
# Author : César Godinho
# Date   : 26/09/2025

class UnsupportedSystemError(Exception):
    """To raise when a the current system is not supported."""
    pass


class InvalidKey(Exception):
    """To raise when an rdb key is invalid."""
    pass
