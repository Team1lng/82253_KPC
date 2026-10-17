###
 # @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @Date: 2024-04-09 08:59:00
 # @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git & please set dead value or install git
 # @LastEditTime: 2024-12-12 08:40:18
 # @FilePath: /2CD-ME/make.sh
 # @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
### 

CC="$(pwd)/compilat_tool/arm-anykav500-linux-uclibcgnueabi/bin/arm-anykav500-linux-uclibcgnueabi-gcc"
CXX="$(pwd)/compilat_tool/arm-anykav500-linux-uclibcgnueabi/bin/arm-anykav500-linux-uclibcgnueabi-g++"

OPTION="AV100-82253-KPC"   # 1户机：键盘+IC卡
ENABLE_ITS=0
ENABLE_ATS=0
ENABLE_CARD=1              # IC卡读卡器
ENABLE_KEYPAD=1            # 数字键盘（SC92F836/XW12A）
ENABLE_FINGERPRINT=0       # 无指纹模块
# isp-gc2083-2024121107.conf
# isp-gc2083-2024121107-reducelight.conf
NIGHT_VERSION="isp-gc2083-2024121107.conf"

#配置音频参数文件
add_audio_param_file()
{
    filename="src/AudioParam.c"

    # 使用sed检查文件中是否有指定的宏定义，并替换为头文件包含
    sed -i 's/#define _AK_AUDIO_CONFIG_H_/#include "ak_common_audio.h"/' $filename
}

config_night_version()
{
    cp -f night_version/$NIGHT_VERSION isp_gc2083_mipi_2lane_av100.conf
}

#选择编译版本类型
SelectCompileVersion()
{
    # 默认 Release，不再交互选择
    RELEASE="y"
    echo "Build For Release"
}

#编译
compile_cmake()
{
    cd build

    rm -rf CMakeFiles
    rm -f cmake_install.cmake
    rm -f CMakeCache.txt
    cmake \
    -DCMAKE_C_COMPILER=${CC} \
    -DCMAKE_CXX_COMPILER=${CXX} \
    -DRELEASE_VERSION=${RELEASE} \
    -DIPC_MODEL="$OPTION" \
    -DITS_ENABLE=${ENABLE_ITS} \
    -DATS_ENABLE=${ENABLE_ATS} \
    -DCARD_ENABLE=${ENABLE_CARD}  \
    -DKEYPAD_ENABLE=${ENABLE_KEYPAD} \
    -DFINGERPRINT_ENABLE=${ENABLE_FINGERPRINT} \
    .
    
    make clean
    make -j16
    cd -
}

#制作升级文件
make_upgrade_package()
{
    cd upgrade/
    ./make_image.sh $OPTION
    cd -
}


clear

add_audio_param_file

config_night_version

SelectCompileVersion

compile_cmake

make_upgrade_package

# cp README.md upgrade/ -f