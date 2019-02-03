# -*- mode: python -*-

# To use as: pyinstaller samples/pyluxcoretool/pyluxcoretool.linux.spec

block_cipher = None


a = Analysis(['pyluxcoretool.py'],
             pathex=['../..'],
             binaries=[
				('../../lib/pyluxcore.so', '.'),
				('../../lib/pyluxcoretools.zip', '.'),
				('../../../target-64-sse2/lib/libOpenImageDenoise.so.0', '.'),
				('../../../target-64-sse2/lib/libembree3.so.3', '.'),
				('../../../target-64-sse2/lib/libtbb.so.2', '.'),
				('../../../target-64-sse2/lib/libtbbmalloc.so.2', '.')
			 ],
             datas=[],
# 'PySide.QtCore','PySide.QtGui' break boot::lexical_cast on Linux. This makes
# PyInstaller pretty much unasable. See https://github.com/LuxCoreRender/LuxCore/issues/80
             hiddenimports=['uuid', 'PySide.QtCore','PySide.QtGui', 'numpy'],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher)
pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          name='pyluxcoretool',
          debug=False,
          strip=False,
          upx=True,
          runtime_tmpdir=None,
          console=True )
