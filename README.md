# Overview
This project is an alternative approach to interfacing with DALSA Cameras on python. The camera I used was Genie Nano. The [pygigev](https://pypi.org/project/pygigev/) module is not very reliable when performing realtime imaging. Debugging the pygigev module has proven to be a considerable challenge. Instead of having python process the images, I had decided to use cpp to process the images and then pass the data to a socket which a python program can then read the data from.

# Quicktart
1. Ensure that you have properly setup the [Linux GigE-V framework](https://www.teledynedalsa.com/en/products/imaging/vision-software/linux-gige-v-framework/). 
2. Once set up you should be able to run make in the `/cpp` folder
```
$ cd ./cpp
$ make
```
3. Execute the `./cpp/genicam` program. The program will create a file socket which you can use a python script to read from
```
# if you are in the ./cpp folder

$ ./genicam
```
4. Once done you can clean up the files if necessary
```
# if you are in the ./cpp folder

$ make clean
```