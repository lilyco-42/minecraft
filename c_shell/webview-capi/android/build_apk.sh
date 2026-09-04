#!/bin/bash
set -e
SDK=${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}
BT=$SDK/build-tools/36.0.0
PLAT=$SDK/platforms/android-36/android.jar
JDK=${JAVA_HOME:-/usr/lib/jvm/temurin-17-jdk}/bin

cd "$(dirname "$0")"
rm -rf out 2>/dev/null || true
mkdir -p out/classes out/dex

echo "[1/5] aapt2 link..."
"$BT/aapt2" link -o out/base.apk -I "$PLAT" --manifest AndroidManifest.xml --java out

echo "[2/5] javac compile..."
"$JDK/javac" --release 8 -classpath "$PLAT" -d out/classes \
  java/local/mc/console/MainActivity.java out/local/mc/console/R.java

echo "[3/5] d8 dex..."
"$BT/d8" --release --lib "$PLAT" --output out/dex out/classes/local/mc/console/*.class

echo "[4/5] zip dex into apk..."
( cd out/dex && zip -q ../base.apk classes.dex )

echo "[5/5] align + sign..."
"$BT/zipalign" -f 4 out/base.apk out/aligned.apk
if [ ! -f out/debug.keystore ]; then
  "$JDK/keytool" -genkeypair -keystore out/debug.keystore -storepass android \
    -alias androiddebugkey -keypass android \
    -dname "CN=Android Debug,O=Android,C=US" -keyalg RSA -keysize 2048 -validity 10000
fi
"$BT/apksigner" sign --ks out/debug.keystore --ks-pass pass:android \
  --key-pass pass:android --out out/mc-console.apk out/aligned.apk

echo "APK_OK: out/mc-console.apk"
ls -la out/mc-console.apk
