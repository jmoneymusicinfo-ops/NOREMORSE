"""Copy native JUCE's CMake config beside itself for a cross-build.
Preserve the installation prefix (native juceaide + JUCE sources), but let
JUCE compile the VST3 manifest helper for the target rather than importing a
prebuilt Linux module-loader helper. Use a private writable JUCE installation.
"""
from pathlib import Path
import shutil
import sys
source = Path(sys.argv[1]).resolve()
target = source.with_name(source.name + '-cross')
if not (source / 'JUCEConfig.cmake').is_file():
    raise SystemExit('Expected the installed native JUCE CMake package directory')
if target.exists():
    if not (target / '.jmoney-cross-package').exists():
        raise SystemExit('Refusing to replace an unrelated existing directory')
    shutil.rmtree(target)
shutil.copytree(source, target)
for name in ['LV2_HELPER.cmake', 'VST3_HELPER.cmake']:
    (target / name).write_text('# Cross-build: no prebuilt host module-loader helper imported.\n')
(target / '.jmoney-cross-package').write_text('JUCE 8.0.6 cross-build helper config\n')
print(target)
