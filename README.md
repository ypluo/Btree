## btree

This is small project contains tree types of btrees: normal btree, btree(unsort node), btree(unsort node with indirect vector)

#### Usage
Test the three tree.

```sh
make

./test
> usage: ./test --scale=int [options] ...
> options:
>   -s, --scale    number of records to insert (int)
>   -t, --tree     the tree type (int [=1])
>   -?, --help     print this message

./test --scale 1000 --tree 2 # test btree(unsort node)

```