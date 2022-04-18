#!/bin/bash

cd test
rm -rf *

echo ""
echo "testing file creation with touch and directory creation with mkdir:"
echo ""
ls
touch abc
touch bbc
mkdir cdc
ls -l

echo ""
echo "testing long file creation:"
echo ""
touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname
touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou
touch eeff
ls -l

echo ""
echo "testing file deletion:"
echo ""
rm -f abc
rm -f bbc
rm -f eeff
ls -l

echo ""
echo "testing directory deletion:"
echo ""
rmdir cdc
ls
mkdir ddd
cd ddd
touch lol
mkdir www
ls
cd ..
ls -l
rmdir ddd

echo ""
echo "testing rm -rf to delete everything:"
echo ""
rm -rf ddd
ls -la
