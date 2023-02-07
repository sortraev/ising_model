gcc_flags=-g -lm -pthread -O3 -Wall -Wextra -pedantic

EXE=ising_model
EXE2=$(EXE)2

SRC=$(EXE).c
SRC2=$(EXE2).c


full: $(EXE)
	./$< $$(tput lines) $$(tput cols)

run: $(EXE)
	./$< 44 80

run2: $(EXE2)
	./$< 48 160




$(EXE): $(SRC)
	gcc $(gcc_flags) $< -o $@

$(EXE2): $(SRC2)
	gcc $(gcc_flags) $< -o $@

clean:
	rm -rf $(EXE)
