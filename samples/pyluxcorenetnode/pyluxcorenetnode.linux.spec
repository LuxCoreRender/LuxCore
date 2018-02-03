# -*- mode: python -*-

# To use as: pyinstaller samples/pyluxcorenetnode/pyluxcorenetnode.linux.spec

block_cipher = None


a = Analysis(['pyluxcorenetnode.py'],
             pathex=['../..'],
             binaries=[
				('../../lib/pyluxcore.so', '.'),
				('../../lib/pyluxcoretools.zip', '.'),
				('../../../LinuxCompile/target-64-sse2/lib/libembree.so.2', '.'),
				('../../../LinuxCompile/target-64-sse2/lib/libtbb.so.2', '.'),
				('../../../LinuxCompile/target-64-sse2/lib/libtbbmalloc.so.2', '.')
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
          name='pyluxcorenetnode',
          debug=False,
          strip=False,
          upx=True,
          runtime_tmpdir=None,
          console=True )
