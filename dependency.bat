@echo off

pushd .
cd ..

REM Check if the vcpkg directory exists
if not exist ".\vcpkg\" (
    git clone https://github.com/microsoft/vcpkg.git
)

cd vcpkg

REM Update the vcpkg repository
git pull

REM Bootstrap vcpkg (setup vcpkg)
call bootstrap-vcpkg.bat

REM Integrate vcpkg with Visual Studio
vcpkg integrate install

REM Upgrade any installed packages
vcpkg upgrade --no-dry-run

REM Install packages with the specified triplet
vcpkg install "lz4" "fmt" "cryptopp" "redis-plus-plus" "gtest" "libpq" "efsw" "boost-algorithm" "boost-dll" "boost-asio" "boost-json" "boost-system" "boost-container" "boost-filesystem" "boost-process" "sndfile" "libsamplerate" "curl" "cpp-httplib[openssl]" "aws-sdk-cpp[ssm]" "librabbitmq" --triplet x64-windows --recurse

popd