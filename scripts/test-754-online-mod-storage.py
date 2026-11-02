"""Run the existing UWP storage fault tests with the 2.0.3 internal I/O header."""
from pathlib import Path

baseline = Path(__file__).with_name('test-735-online-mod-storage.py')
source = baseline.read_text()
include = '#include "io_uwp.hpp"'
assert source.count(include) == 1
source = source.replace(include, '#include "io_internal.hpp"\n' + include)
exec(compile(source, str(baseline), 'exec'))
