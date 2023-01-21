## RHEL deps
* yum install git, cmake, clang, libsodium-devel, opus, openssl-devel, wget
* install DPP from rpm
```sh
wget -O dpp.rpm https://dl.dpp.dev/latest/linux-x64/rpm
sudo yum localinstall dpp.rpm
```
* [HACK] use cmake flag if error no lib found `-DZLIB_INCLUDE_DIR=/include -DZLIB_LIBRARY=/lib -DOPUS_INCLUDE_DIRS=/include -DOPUS_LIBRARIES=/lib`
* !TODO: Figure out opus not detected
