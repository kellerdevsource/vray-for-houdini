cd ~/build/qt5

~/src/qt-5.6.1-adsk/configure $* \
-prefix ~/install/qt-5.6.1 \
-sdk macosx10.9 \
-opensource \
-confirm-license \
-shared \
-debug-and-release \
-no-cups \
-no-openssl \
-no-audio-backend \
-no-fontconfig \
-no-native-gestures \
-no-dbus \
-plugin-sql-sqlite \
-qt-libjpeg \
-qt-sql-sqlite \
-qt-zlib \
-nomake examples \
-nomake tests
