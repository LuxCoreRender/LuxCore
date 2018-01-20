# -*- mode: python -*-

# To use as: pyinstaller samples/pyluxcoremerge/pyluxcoremerge.linux.spec

block_cipher = None


a = Analysis(['pyluxcoremerge.py'],
             pathex=['../..'],
             binaries=[
				('../../lib/pyluxcore.so', '.'),
				('../../lib/pyluxcoretools.zip', '.')
			 ],
             datas=[],
             hiddenimports=['uuid'],
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
          name='pyluxcoremerge',
          debug=False,
          strip=False,
          upx=True,
          runtime_tmpdir=None,
          console=True )
