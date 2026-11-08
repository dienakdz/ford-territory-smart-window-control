"""NLP processing package for command recognition."""
from .normalizer import normalize
from .profiles import COMMAND_PROFILES, STOP_URGENT_KEYWORDS
from .matcher import compute_similarity_command

__all__ = [
    "normalize",
    "COMMAND_PROFILES",
    "STOP_URGENT_KEYWORDS",
    "compute_similarity_command"
]
