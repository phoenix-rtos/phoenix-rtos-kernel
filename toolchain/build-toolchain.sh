#!/bin/bash
# $1 - toolchain target (e.g. arm-phoenix)
# $2 - toolchain install absolute path (i.e. no "." or ".." in the path)


set -e

log() {
    echo -e "\e[35;1m$*\e[0m"
}

if [ -z "$1" ]; then
    echo "No toolchain target provided! Abort."
    echo "officially supported targets: arm-phoenix i386-pc-phoenix riscv64-phoenix-elf"
    exit 1
fi

if [ -z "$2" ]; then
    echo "No toolchain install path provided! Abort."
    exit 1
fi

# old legacy versions of the compiler:
#BINUTILS=binutils-2.28
#GCC=gcc-7.1.0

BINUTILS=binutils-2.34
GCC=gcc-9.3.0

TARGET="$1"
BUILD_ROOT="$2"
TOOLCHAIN_PREFIX="${BUILD_ROOT}/${TARGET}"
MAKEOPTS="-j9 -s"

log "downloading packages"
mkdir -p "${TOOLCHAIN_PREFIX}"
cp ./*.patch "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

[ ! -f ${BINUTILS}.tar.bz2 ] && wget "http://ftp.gnu.org/gnu/binutils/${BINUTILS}.tar.bz2"
[ ! -f ${GCC}.tar.xz ] && wget "http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/gcc/${GCC}/${GCC}.tar.xz"

log "extracting packages"
[ ! -d ${BINUTILS} ] && tar jxf ${BINUTILS}.tar.bz2
[ ! -d ${GCC} ] && tar Jxf ${GCC}.tar.xz


log "patching ${BINUTILS}"
for patchfile in "${BINUTILS}"-*.patch; do
    if [ ! -f "${BINUTILS}/$patchfile.applied" ]; then
        patch -d "${BINUTILS}" -p1 < "$patchfile"
        touch "${BINUTILS}/$patchfile.applied"
    fi
done

log "building binutils"
rm -rf "${BINUTILS}/build"
mkdir -p "${BINUTILS}/build"
cd "${BINUTILS}/build"

../configure --target="${TARGET}" --prefix="${TOOLCHAIN_PREFIX}" \
             --with-sysroot="${TOOLCHAIN_PREFIX}/${TARGET}" --enable-lto
make ${MAKEOPTS}

log "installing binutils"
make install

cd "${BUILD_ROOT}"

log "downloading GCC dependencies"
(cd "$GCC" && ./contrib/download_prerequisites)


log "patching ${GCC}"
for patchfile in "${GCC}"-*.patch; do
    if [ ! -f "${GCC}/$patchfile.applied" ]; then
        patch -d "${GCC}" -p1 < "$patchfile"
        touch "${GCC}/$patchfile.applied"
    fi
done

log "building GCC"
rm -rf "${GCC}/build"
mkdir -p "${GCC}/build"
cd "${GCC}/build"

if [[ "$TARGET" = *"arm-" ]]; then
    GCC_CONFIG_PARAMS="--with-abi=aapcs"
fi

# GCC compiler options
# --with-sysroot -> cross-compiler sysroot
# --with-newlib -> do note generate standard library includes by fixincludes, do not include _eprintf in libgcc
# --disable-libssp -> stack smashing protector library disabled
# --disable-nls -> all compiler messages will be in english

../configure --target="${TARGET}" --prefix="${TOOLCHAIN_PREFIX}" \
             --with-sysroot="${TOOLCHAIN_PREFIX}/${TARGET}" \
             --enable-languages=c --with-newlib \
             --disable-libssp --disable-nls $GCC_CONFIG_PARAMS

make ${MAKEOPTS} all-gcc all-target-libgcc

log "installing GCC"
make install-gcc install-target-libgcc
