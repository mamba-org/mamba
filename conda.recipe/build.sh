mkdir build
cd build

# Conda's binary relocation can result in string changing which can result in errors like
#    > warning: command substitution: ignored null byte in input
# https://github.com/mamba-org/mamba/issues/1517
export CXXFLAGS="${CXXFLAGS} -fno-merge-constants"
export CFLAGS="${CFLAGS} -fno-merge-constants"
export CXXFLAGS="${CXXFLAGS} -D_LIBCPP_DISABLE_AVAILABILITY=1"

cmake ${CMAKE_ARGS} .. \
         -DCMAKE_INSTALL_PREFIX=${PREFIX} \
         -DCMAKE_BUILD_TYPE="Release" \
         -DBUILD_LIBMAMBA=ON \
         -DBUILD_STATIC_DEPS=ON \
         -DBUILD_MICROMAMBA=ON \
         -DMICROMAMBA_LINKAGE=FULL_STATIC

make -j${CPU_COUNT}
make install

${STRIP:-strip} ${PREFIX}/bin/micromamba
