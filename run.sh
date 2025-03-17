#!/bin/bash
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -kernel build/bin/riscv-pke \
  -append "app_exec"