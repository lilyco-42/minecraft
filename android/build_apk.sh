#!/bin/bash
# 用 Android SDK CLI 工具链打包 WebView APK (无 gradle, 纯 java 源码无资源文件).
# 用法: bash android/build_apk.sh
set -e
SDK=D:/android_sdk
BT=$SDK/build-tools/36.0.0
PLAT=$SDK/platforms/android-36/android.jar
JDK=D:/APP/scoop/apps/temurin21-jdk/current/bin

cd "$(dirname "$0")"
rm -rf out 2>/dev/null || true   # Windows 下 Defender 可能短暂锁文件, 失败则重试
sleep 1
rm -rf out 2>/dev/null || true
mkdir -p out/classes out/dex

# 1) aapt2 link: manifest + 平台资源 -> 基础 apk + R.java (无自有资源)
"$BT/aapt2.exe" link -o out/base.apk -I "$PLAT" --manifest AndroidManifest.xml --java out

# 2) javac 编译 (源码 + 生成的 R.java)
"$JDK/javac.exe" --release 8 -classpath "$PLAT" -d out/classes \
  java/local/mc/console/MainActivity.java out/local/mc/console/R.java

# 3) d8 转 dex
"$BT/d8.bat" --release --lib "$PLAT" --output out/dex out/classes/local/mc/console/*.class

# 4) dex 塞进 apk (out/dex 的上一级就是 out/base.apk)
( cd out/dex && zip -q ../base.apk classes.dex )
unzip -l out/base.apk | grep -q classes.dex  # 防御: dex 必须真进了包

# 5) 对齐 + 签名
"$BT/zipalign.exe" -f 4 out/base.apk out/aligned.apk
if [ ! -f out/debug.keystore ]; then
  "$JDK/keytool.exe" -genkeypair -keystore out/debug.keystore -storepass android \
    -alias androiddebugkey -keypass android -dname "CN=Android Debug,O=Android,C=US" \
    -keyalg RSA -keysize 2048 -validity 10000
fi
"$BT/apksigner.bat" sign --ks out/debug.keystore --ks-pass pass:android \
  --key-pass pass:android --out out/mc-console.apk out/aligned.apk
echo "APK_OK: out/mc-console.apk"
