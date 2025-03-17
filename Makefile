run: 
	qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -kernel build/bin/riscv-pke \
  -append "app_exec"

gdb:
	qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -kernel build/bin/riscv-pke \
  -append "app_exec" \
	-s -S

gdb-client:
	riscv64-unknown-elf-gdb -ex "target remote localhost:1234" build/bin/riscv-pke -q