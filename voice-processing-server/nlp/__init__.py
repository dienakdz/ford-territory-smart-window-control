"""NLP processing package for command recognition."""
from .normalizer import normalize
from .profiles import COMMAND_PROFILES
from .matcher import compute_similarity_command

__all__ = [
    "normalize",
    "COMMAND_PROFILES",
    "compute_similarity_command"
]
