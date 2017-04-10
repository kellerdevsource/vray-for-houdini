cd C:\build\qt5

C:\src\libs\qt5\configure.bat ^
-prefix C:\src\vray_for_houdini_sdk_windows\hdk\qt\5.6.1 ^
-platform win32-msvc2015 ^
-qmake ^
-opensource ^
-confirm-license ^
-shared ^
-release ^
-mp ^
-opengl dynamic ^
-no-cups ^
-no-openssl ^
-no-audio-backend ^
-no-fontconfig ^
-no-native-gestures ^
-no-dbus ^
-plugin-sql-sqlite ^
-qt-libjpeg ^
-qt-sql-sqlite ^
-qt-zlib ^
-nomake examples ^
-nomake tests ^
%*
