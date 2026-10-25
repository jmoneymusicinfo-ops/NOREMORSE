"""Package a built Windows VST3 plus its complete open-source build sources."""
import argparse
import hashlib
import re
import zipfile
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--build-dir',default='build-win')
parser.add_argument('--output-dir',default='downloads')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
build=root/args.build_dir
out=root/args.output_dir
out.mkdir(exist_ok=True)
version=re.search(r'project\s*\([^)]*\bVERSION\s+([0-9.]+)',(root/'CMakeLists.txt').read_text())[1]
bundle=build/'JMoney_artefacts/Release/VST3/J Money - Vocal Intelligence EQ.vst3'
module=bundle/'Contents/x86_64-win/J Money - Vocal Intelligence EQ.vst3'
if not module.is_file() or module.read_bytes()[:2]!=b'MZ':raise SystemExit('Built Windows VST3 missing')
name=f'JMoney-Vocal-Intelligence-EQ-Windows-x64-v{version}.zip'
with zipfile.ZipFile(out/name,'w',zipfile.ZIP_DEFLATED) as z:
    for f in sorted(bundle.rglob('*')):
        if f.is_file():z.write(f,str(f.relative_to(bundle.parent)))
    for item in ['LICENSE','THIRD_PARTY_NOTICES.md']:
        z.write(root/item,item)
    z.write(root/'docs/INSTALL-WINDOWS.txt','INSTALL-WINDOWS.txt')
    for folder in ['docs','presets']:
        for f in sorted((root/folder).rglob('*')):
            if f.is_file():z.write(f,str(f.relative_to(root)))
source_name=f'JMoney-Vocal-Intelligence-EQ-source-v{version}.zip'
juce=build/'_deps/juce-src'
if not (juce/'LICENSE.md').exists():raise SystemExit('Pinned JUCE source missing for source package')
with zipfile.ZipFile(out/source_name,'w',zipfile.ZIP_DEFLATED) as z:
    for item in ['CMakeLists.txt','LICENSE','THIRD_PARTY_NOTICES.md','README.md','.gitignore','.clang-format']:
        z.write(root/item,'JMoney-Vocal-Intelligence-EQ/'+item)
    for folder in ['Source','tests','scripts','cmake','docs','presets','.github']:
        for f in sorted((root/folder).rglob('*')):
            if f.is_file() and '__pycache__' not in f.parts:
                z.write(f,'JMoney-Vocal-Intelligence-EQ/'+str(f.relative_to(root)))
    for f in sorted(juce.rglob('*')):
        r=f.relative_to(juce)
        if f.is_file() and '.git' not in r.parts:
            z.write(f,'JMoney-Vocal-Intelligence-EQ/third_party/JUCE/'+str(r))
    z.writestr('JMoney-Vocal-Intelligence-EQ/third_party/JUCE-COMMIT.txt','51a8a6d7aeae7326956d747737ccf1575e61e209 plus documented compatibility patches\n')
for filename,alias in [(name,'JMoney-Vocal-Intelligence-EQ-Windows-x64.zip'),(source_name,'JMoney-Vocal-Intelligence-EQ-source.zip')]:
    data=(out/filename).read_bytes()
    (out/alias).write_bytes(data)
    with zipfile.ZipFile(out/filename) as z:
        if z.testzip() is not None:raise SystemExit('Invalid release archive')
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n' for f in sorted(out.glob('*.zip'))))
print('Packaged Windows VST3 version',version)
