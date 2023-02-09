# Tiny parameterised processor in C

## Features

0. Parameterised design
0. Set-associative cache with random replacement policy
0. Cache-cost-sensitive performance estimator

## Assembly language
- `@L`: define label `L`
- `set addr val`: assign `mem[addr]` to `val` where val must be non-negative
- `sub addr0 addr1`: set `mem[addr0]` to `mem[addr0]-mem[addr1]`
- `bnz addr L`: jump to `L` if `mem[addr]` is not 0
