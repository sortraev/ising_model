gcc_flags=-g -lm -pthread -lcurses -O3 -Wall -Wextra -pedantic

EXE=ising_model
SRC=$(EXE).c

compile: $(EXE) run

run: $(EXE)
	./$<

$(EXE): $(SRC)
	gcc $(gcc_flags) $< -o $@

clean:
	rm -rf $(EXE)
