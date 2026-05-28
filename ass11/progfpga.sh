sudo modprobe spi-bcm2835 -r
yosys -p 'synth_ice40 -top top_entity -json ice40.json' TopEntity.v
nextpnr-ice40 --hx8k --json ice40.json --pcf ico-jiwy.pcf --asc ice40.asc
icepack ice40.asc ice40.bin
../../icoprog/icoprog -R
../../icoprog/icoprog -p < ice40.bin
sudo modprobe spi-bcm2835