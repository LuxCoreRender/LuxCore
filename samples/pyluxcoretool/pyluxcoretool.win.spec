# -*- mode: python -*-

# To use as: pyinstaller samples/pyluxcoretool/pyluxcoretool.win.spec

block_cipher = None

def pyside_imports():
    try:
        import PySide.QtCore as QtCore
        result = ['PySide.QtCore','PySide.QtGui']
    except ImportError:
        from PySide2 import QtCore
        result = ['PySide2.QtCore','PySide2.QtGui', 'PySide2.QtWidgets']
    return result

a = Analysis(['pyluxcoretool.py'],
             pathex=['../..'],
             binaries=[
				('../../../WindowsCompile/Build_CMake/LuxCore/lib/Release/pyluxcore.pyd', '.'),
				('../../../WindowsCompile/Build_CMake/LuxCore/lib/pyluxcoretools.zip', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/OpenImageDenoise.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/embree3.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/tbb.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/tbbmalloc.dll', '.'),
				('../../../WindowsCompileDeps/x64/Release/lib/OpenImageIO.dll', '.')
			 ],
             datas=[],
             hiddenimports=['uuid', 'numpy'] + pyside_imports(),
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
          console=True )
