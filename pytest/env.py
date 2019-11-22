#!/usr/bin/env python
from rdc import env
print(env.find('PATH'))
print(env.get_env('INT', 0))
