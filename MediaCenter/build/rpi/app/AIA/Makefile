.PHONY: main clean debug debugclean

main:
	@./configure.sh || exit 1
	@echo "CONFIG=Release">>config.make
	@echo "CFLAGS += -Wall -Os">>config.make
	@cp config.make ../../ -rf
	make -f Rules.make

clean:
	@./configure.sh || exit 1
	@echo "CONFIG=Release">>config.make
	@cp config.make ../../ -rf
	make -f Rules.make clean

debug:
	@./configure.sh || exit 1
	@echo "CONFIG=Debug">>config.make
	@echo "CFLAGS += -Wall -g">>config.make
	@cp config.make ../../ -rf
	make -f Rules.make
	
debugclean:
	@./configure.sh || exit 1
	@echo "CONFIG=Debug">>config.make
	@cp config.make ../../ -rf
	make -f Rules.make clean
	