# Instructions

## Compilation

To compile the program, use the following command:

```bash
gcc sacayanan_ra5.c -lm -pthread
```

> **Note:** Some operating systems may require the `-lm` and `-pthread` flags during compilation.

## Running the Program

The program will prompt you to input the following parameters:

- `n`: Size of the matrix.
- `p`: Port to which the program will bind.
- `s`: Role of the program (`0` for master, `1` for slave).

### Sample Run

```bash
./a.out
20000 4022 0
```

## Configuration File

A `config.txt` file is required for the program to run. The file should have the following structure:

2. Each line should contain the IP address and port of a slave, separated by a colon (`:`).
3. Ensure the file ends with a newline (`\n`) character.

### Example `config.txt`

```
127.0.0.1:4022
127.0.0.1:4023
127.0.0.1:4024
127.0.0.1:4025
127.0.0.1:4026

```
