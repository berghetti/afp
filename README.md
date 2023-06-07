
## AFP

### Install prerequisits and run setup script
    ./install_prerequisites.sh
    ./scripts/setup.sh

### Compile
    make            # AFP Library
    make submodules # dpdk and rocksdb
    make database   # rocksdb database create


### DIRS
    database: database rocksdb
    deps: dependencys
    src: source code to library AFP
    scripts: general scripts
    tests: general tests
