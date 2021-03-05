FLAGS:=-fmax-errors=5

all: test.cc btree.h btree_unsort.h slotonly.h base.h
	g++ $(FLAGS) -o test test.cc

debug: test.cc btree.h btree_unsort.h slotonly.h base.h
	g++ $(FLAGS) -g -o debug test.cc


clean:
	rm *.exe
	rm test