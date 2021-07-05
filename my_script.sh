#/bin/sh -v
#
# My test script to test my submission for pfind.
# It makes the program, performs a few sample tests
# of my own, and runs the lib215-provided test script.
#

#-------------------------------------
#    compile program
#-------------------------------------
make clean

make pfind

#-------------------------------------
#    create tmp files to store output
#-------------------------------------

touch my.output
touch find.output

#-------------------------------------
#    my positive tests
#-------------------------------------

#------------------------------------------
# prints the starting path
#

./pfind . -name .

find . -name .

#------------------------------------------
# creates a subdirectory, and searches ".."
#

mkdir burrow
cd burrow

../pfind .. > ../my.output

find .. > ../find.output

# compare output
diff ../my.output ../find.output

# go back and remove test directory
cd ..
rm -r burrow

#------------------------------------------
# test a long directory name
#

./pfind ~lib215/hw/pfind/pft.d -name cookie > my.output

find ~lib215/hw/pfind/pft.d -name cookie > find.output

# compare output
diff my.output find.output

#------------------------------------------
# create and search for symbolic link
#

ln -s pfind.c pfind-link.tmp

./pfind . -type l

find . -type l

rm pfind-link.tmp

#-------------------------------------
#    my negative tests
#-------------------------------------

# demo pathname out of order
./pfind -name foo .

find -name foo .

# invalid type option
./pfind -type q

find -type q

# pathname cannot start with '-', treated as "-option"
./pfind -foopath

find -foopath

#-------------------------------------
#    remove tmp files
#-------------------------------------
rm my.output find.output

#-------------------------------------
#    check memory usage
#-------------------------------------
valgrind --tool=memcheck --leak-check=yes ./pfind .

#-------------------------------------
#    run the course test-script
#-------------------------------------
~lib215/hw/pfind/test.pfind
