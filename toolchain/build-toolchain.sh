#!/bin/bash
# $1 - toolchain target (e.g. arm-phoenix)
# $2 - toolchain install path

set -e

if [ -z "$1" ]; then
    echo "No toolchain target provided! Abort."
    exit 1
fi

if [ -z "$2" ]; then
    echo "No toolchain install path provided! Abort."
    exit 1
fi

BINUTILS=binutils-2.28
GMP=gmp-6.1.2
MPFR=mpfr-3.1.5
MPC=mpc-1.0.3
GCC=gcc-7.1.0

TARGET=$1
BUILD_ROOT="$2"
TOOLCHAIN_PREFIX="${BUILD_ROOT}/${TARGET}"
MAKEOPTS="-j9 -s"

# Download packages
mkdir -p "${TOOLCHAIN_PREFIX}"
cp ./*.patch "${BUILD_ROOT}"
cd "${BUILD_ROOT}"

[[ ! -f ${BINUTILS}.tar.bz2 ]] && wget "http://ftp.gnu.org/gnu/binutils/${BINUTILS}.tar.bz2"
[[ ! -f ${GMP}.tar.bz2 ]] && wget "http://ftp.gnu.org/gnu/gmp/${GMP}.tar.bz2"
[[ ! -f ${MPFR}.tar.bz2 ]] && wget "http://ftp.gnu.org/gnu/mpfr/${MPFR}.tar.bz2"
[[ ! -f ${MPC}.tar.gz ]] && wget "ftp://ftp.gnu.org/gnu/mpc/${MPC}.tar.gz"
[[ ! -f ${GCC}.tar.bz2 ]] && wget "http://www.mirrorservice.org/sites/ftp.gnu.org/gnu/gcc/${GCC}/${GCC}.tar.bz2"

# Extract packages
[[ ! -d ${BINUTILS} ]] && tar jxf ${BINUTILS}.tar.bz2 && mkdir "${BINUTILS}/build"
[[ ! -d ${GMP} ]] && tar jxf ${GMP}.tar.bz2 && mkdir "${GMP}/build"
[[ ! -d ${MPFR} ]] && tar jxf ${MPFR}.tar.bz2 && mkdir "${MPFR}/build"
[[ ! -d ${MPC} ]] && tar xf ${MPC}.tar.gz && mkdir "${MPC}/build"
[[ ! -d ${GCC} ]] && tar jxf ${GCC}.tar.bz2 && mkdir "${GCC}/build"

# patch Binutils
for patchfile in binutils-*.patch; do
    if [ ! -f "${BINUTILS}/build/$patchfile.applied" ]; then
        patch -d ${BINUTILS} -p1 < "$patchfile"
    touch "${BINUTILS}/build/$patchfile.applied"
    fi
done

# Build Binutils
cd "${BINUTILS}/build"

../configure --target=${TARGET} --prefix="${TOOLCHAIN_PREFIX}" --with-sysroot="${TOOLCHAIN_PREFIX}/${TARGET}"
make ${MAKEOPTS}
make ${MAKEOPTS} install

cd -

# Build GMP
cd "${GMP}/build"

../configure --prefix="${TOOLCHAIN_PREFIX}" --disable-shared
make ${MAKEOPTS}
make ${MAKEOPTS} install

cd -

# Build MPFR
cd "${MPFR}/build"

../configure --prefix="${TOOLCHAIN_PREFIX}" --with-gmp="${TOOLCHAIN_PREFIX}" --disable-shared
make ${MAKEOPTS}
make ${MAKEOPTS} install

cd -

# Build MPC
cd "${MPC}/build"

../configure --target=${TARGET} --prefix="${TOOLCHAIN_PREFIX}" --with-gmp="${TOOLCHAIN_PREFIX}" --with-mpfr="${TOOLCHAIN_PREFIX}" --disable-shared
make ${MAKEOPTS}
make ${MAKEOPTS} install

cd -

# patch GCC
for patchfile in gcc-*.patch; do
    if [ ! -f "${GCC}/build/$patchfile.applied" ]; then
        patch -d ${GCC} -p1 < "$patchfile"
    touch "${GCC}/build/$patchfile.applied"
    fi
done

# Build GCC
cd "${GCC}/build"

if [[ "$TARGET" = *"arm-" ]]; then
    GCC_CONFIG_PARAMS="--with-abi=aapcs"
fi


../configure --target=${TARGET} --prefix="${TOOLCHAIN_PREFIX}" --with-sysroot="${TOOLCHAIN_PREFIX}/${TARGET}" \
             --with-gmp="${TOOLCHAIN_PREFIX}" --with-mpfr="${TOOLCHAIN_PREFIX}" --with-mpc="${TOOLCHAIN_PREFIX}" \
             --enable-languages=c --with-newlib \
             --disable-libssp --disable-nls $GCC_CONFIG_PARAMS

make ${MAKEOPTS} all-gcc all-target-libgcc
make ${MAKEOPTS} install-gcc install-target-libgcc

exit 0
