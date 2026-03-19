"""Entry point for ``python -m keystone`` invocation (issue #106)."""

import asyncio
import sys

from keystone.daemon import _parse_args, main

args = _parse_args()
try:
    asyncio.run(main(args))
except KeyboardInterrupt:
    sys.exit(0)
