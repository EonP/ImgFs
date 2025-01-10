# ImgFs

Image File System with CLI tools and multithreaded web server

## Prerequisites

First note that this project only runs on a Linux machine. Run the following commands to install the requirements: 
```bash
sudo apt install check pip pkg-config libvips-dev libjson-c-dev
```
```bash
pip install parse robotframework
```

## Compilating the project 

From the `done` directory, run 
```bash
make all
```
## CLI Tools 

```bash
> ./imgfscmd help
imgfscmd [COMMAND] [ARGUMENTS]
  help: displays this help.
  list <imgFS_filename>: list imgFS content.
  create <imgFS_filename> [options]: create a new imgFS.
      options are:
          -max_files <MAX_FILES>: maximum number of files.
                                  default value is 128
                                  maximum value is 4294967295
          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.
                                  default value is 64x64
                                  maximum value is 128x128
          -small_res <X_RES> <Y_RES>: resolution for small images.
                                  default value is 256x256
                                  maximum value is 512x512
  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]: reads an image from the imgFS and save it to a file. Default resolution is "original".
  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.
  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.
```

## Multithreaded Web Server

To run the web server: 
```bash
cp ../provided/src/index.html .
imgfs_server <ImgFS file> <port number>
```

