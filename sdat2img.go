package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

type block_command struct {
	command string
	blocks  []block_range
}

type block_range struct {
	a int
	b int
}

const BLOCK_SIZE = 4096

func print_help() {
	fmt.Println("./sdat2img <system.transfer.list> <system.new.dat> [system.img]")
}

func xmax(r []block_range) int {
	n := 0
	for _, cur := range r {
		if cur.a > n || cur.b > n {
			if cur.a > cur.b {
				n = cur.a
			} else {
				n = cur.b
			}
		}
	}
	return n * BLOCK_SIZE
}

func rangeset(s string) []block_range {
	var r []block_range
	src_set := strings.Split(s, ",")
	var num_set []int

	for _, c := range src_set {
		n, _ := strconv.Atoi(c)
		num_set = append(num_set, n)
	}

	if len(num_set) != num_set[0]+1 {
		fmt.Fprintln(os.Stderr, "Error on parsing following data to rangeset:\n", s)
		os.Exit(1)
	}

	for i := 1; i < len(num_set); i += 2 {
		r = append(r, block_range{num_set[i], num_set[i+1]})
	}

	return r
}

func parse_transfer_list_file(flist io.ReadCloser) (int, int, []block_command) {
	var version, new_blocks int
	var commands []block_command

	isdigit := func(s string) bool {
		_, err := strconv.Atoi(s)
		return err == nil
	}

	scanner := bufio.NewScanner(flist)

	scanner.Scan()
	version, _ = strconv.Atoi(scanner.Text())
	scanner.Scan()
	new_blocks, _ = strconv.Atoi(scanner.Text())

	if version >= 2 {
		scanner.Scan()
		scanner.Scan()
	}

	var line string
	for scanner.Scan() {
		line = scanner.Text()

		s := strings.Split(line, " ")
		cmd := s[0]

		if cmd == "erase" || cmd == "new" || cmd == "zero" {
			commands = append(commands, block_command{cmd, rangeset(s[1])})
		} else {
			if !isdigit(cmd[0:1]) {
				fmt.Fprintln(os.Stderr, "Command ", cmd, " is not valid.")
				flist.Close()
				os.Exit(1)
			}
		}

	}

	return version, new_blocks, commands
}

func main() {

	var transfer_list_file, new_dat_file, output_file string
	var err error

	if len(os.Args) == 4 {
		transfer_list_file = os.Args[1]
		new_dat_file = os.Args[2]
		output_file = os.Args[3]
	} else {
		print_help()
		os.Exit(1)
	}

	flist, err := os.OpenFile(transfer_list_file, os.O_RDONLY, 0644)
	if err != nil {
		fmt.Println("Error: ", err.Error())
		return
	}
	defer flist.Close()
	finfo, err := flist.Stat()
	if finfo.IsDir() {
		fmt.Println("Error: transfer.list is not a file but dir.")
		return
	}

	var version int
	var commands []block_command

	version, _, commands = parse_transfer_list_file(flist)

	if version == 1 {
		fmt.Println("Android Lollipop 5.0 detected!")
	} else if version == 2 {
		fmt.Println("Android Lollipop 5.1 detected!")
	} else if version == 3 {
		fmt.Println("Android Marshmallow 6.x detected!")
	} else if version == 4 {
		fmt.Println("Android Nougat 7.x / Oreo 8.x detected!")
	} else {
		fmt.Println("Unknown Android version!")
	}

	fdat, err := os.OpenFile(new_dat_file, os.O_RDONLY, 0644)
	if err != nil {
		fmt.Println("Error: ", err.Error())
		return
	}
	defer fdat.Close()
	finfo, err = fdat.Stat()
	if finfo.IsDir() {
		fmt.Println("Error: new.dat is not a file but dir.")
		return
	}

	fout, err := os.Create(output_file)
	if err != nil {
		fmt.Println("Error: ", err.Error())
		return
	}
	defer fout.Close()

	var all_block_sets []block_range
	for _, cur := range commands {
		all_block_sets = append(all_block_sets, cur.blocks...)
	}

	max_file_size := xmax(all_block_sets)
	var begin, end, block_count int
	buf := make([]byte, 4096)

	//fdat_reader := bufio.NewReader(fdat)
	//fout_writer := bufio.NewWriter(fout)

	for _, command := range commands {
		if command.command == "new" {
			for _, block := range command.blocks {
				begin = block.a
				end = block.b
				block_count = end - begin

				fmt.Println("Copying ", block_count, " blocks into position ", begin, "...")

				fout.Seek(int64(begin*BLOCK_SIZE), io.SeekStart)

				for block_count > 0 {
					fdat.Read(buf)
					fout.Write(buf)
					block_count--
				}
			}
		} else {
			fmt.Println("Skipping command ", command.command, "...")
		}
	}

	pos, _ := fout.Seek(0, io.SeekCurrent)
	if pos < int64(max_file_size) {
		fout.Truncate(int64(max_file_size))
	}

	abs, _ := filepath.Abs(fout.Name())
	fmt.Println("Done! Output image: ", abs)
}
