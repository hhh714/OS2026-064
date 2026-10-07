set pagination off
set confirm off
set architecture riscv:rv64
target remote 127.0.0.1:12345
echo === Reset vector ===\n
info registers pc
x/10i 0x1000
echo === Kernel already loaded before guest execution ===\n
x/4i 0x80200000
si
si
si
si
echo === Before reset ROM jump ===\n
info registers pc t0 a0 a1
si
echo === Firmware entry ===\n
info registers pc
x/5i $pc
break *0x80200000
continue
echo === Kernel entry ===\n
info registers pc sp ra
x/5i $pc
break kern_init
continue
echo === C entry and stack ===\n
info registers pc sp ra
p/x &bootstacktop
detach
quit
