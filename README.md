
## AFP

### Install prerequisits
    ./install_prerequisites.sh

### Compile AFP
    make -j $(nproc) -C $ROOT_PATH static_libake
    make submodules
    make database


### DIRS
    database: database rocksdb
    deps: dependencys
    src: source code to library AFP
    scripts: general scripts
