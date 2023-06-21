define tpiuconf
	monitor stm32f1x.tpiu configure -protocol uart -output - -traceclk 72000000 -formatter 0
	monitor stm32f1x.tpiu enable
end

define flash
	monitor program build/nes_to_usb.elf reset
end
