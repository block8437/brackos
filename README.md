![brackos logo](https://github.com/block8437/brackos/blob/master/res/logo.png)

# brackos
fun kernel experimentations

## TODO

- [x] vasa waste node
- [x] ring buffer
- [ ] test waste node
- [ ] test big alloc
- [ ] yield/rwlock
-  net
	- [ ] rtl8139 to ring buffer
	- [ ] r/w from ring buffer using yield
	- [ ] ethernet 
	- [ ] arp
	- [ ] udp

## Building

    make clean && make && make image

## Running

    make run
