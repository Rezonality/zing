git pull
cd vcpkg
git checkout master
git pull
cd ..
cd libs/zest
git checkout main
git pull
cmd /c subs.bat
cd ..
