
MOD=kmod_ipi

sudo rmmod $MOD; make && sudo insmod $MOD.ko && sudo chmod 666 /dev/$MOD
