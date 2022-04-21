#!/bin/bash

cd test
# rm -rf *

echo "run ls -la to show what we have at first:"
ls -la
echo ""
echo "testing file creation with touch (abc and bbc) and directory creation with mkdir (cdc):"
touch abc
touch bbc
mkdir cdc
echo "now we have:"
ls -a

echo ""
echo "testing long name file creation:"
echo "creating mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname:"
touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongname
echo ""
echo "creating mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou:"
touch mymomsaysthisfileistoolongwhydowecreateafilewithsuchalongnamewhatiswrongwithyou
echo ""
echo "creating eeff"
touch eeff
echo "now we have:"
ls -a

echo ""
echo "testing file deletion:"
echo "deleting abc, bbc, and eeff"
rm -f abc
rm -f bbc
rm -f eeff
echo "now we have:"
ls -a

echo ""
echo "testing directory deletion:"
echo "deleting cdc"
rmdir cdc
echo "now we have:"
ls -a
echo ""
echo "creating directory ddd with a subdirectory www, and a file lol in ddd:"
mkdir ddd
cd ddd
touch lol
mkdir www
echo "now in ddd (ls -l ddd) we have:"
cd ..
ls -l ddd
echo ""
echo "now in test (ls -l) we have:"
ls -l
echo ""
echo "deleting ddd"
rmdir ddd

echo ""
echo "testing rm -rf to delete everything:"
echo "before deletion we have:"
ls -a
rm -rf ddd
echo "after deletion we now have:"
ls -a
