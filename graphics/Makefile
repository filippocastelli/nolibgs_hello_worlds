.PHONY: all cleansub
all:
	mkpsxiso -y ./isoconfig.xml
	/home/phil/repos/cue2pops-linux/cue2pops /home/phil/repos/nolibgs_hello_worlds/graphics/data_cd.cue
	iat data_cd.bin data_cd.iso
cleansub:
	$(MAKE) clean
	rm -f data_cd.cue data_cd.bin data_cd.VCD data_cd.iso

TARGET = graphics

SRCS = graphics.c #\
#../TIM/tornabene4.tim \

include ../common.mk 
