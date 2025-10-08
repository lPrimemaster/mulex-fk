# Brief  : Defines the logger for python
# Author : César Godinho
# Date   : 26/09/2025

import logging

logger = logging.getLogger(__name__)
logger.addHandler(logging.NullHandler())
