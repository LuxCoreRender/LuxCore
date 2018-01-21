# -*- mode: python -*-

# To use as: pyinstaller samples/pyluxcoreconsole/pyluxcoreconsole.win.spec

block_cipher = None


a = Analysis(['pyluxcoreconsole.py'],
             pathex=['../..'],
             binaries=[
				('../../../WindowsCompile/Build_CMake/LuxCore/lib/Release/pyluxcore.pyd', '.'),
				('../../lib/pyluxcoretools.zip', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/embree.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/tbb.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/OpenImageIO.dll', '.')
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
          name='pyluxcoreconsole',
          debug=False,
          strip=False,
          upx=True,
          console=True )

