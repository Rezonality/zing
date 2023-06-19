git pull
git submodule update --init --recursive
cd libs/zest
git checkout main
git pull
cmd /c subs.bat
cd ..
